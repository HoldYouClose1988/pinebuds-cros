/***************************************************************************
 * CROS latency probe accumulators — local wall-clock hops only.
 *
 * Bud clocks are not synchronized; E2E air time is NOT measured here.
 * Goal: split the non-jitter “other ~80 ms” into capture→send, BT queue,
 * L2CAP TX_HANDLED, recv→put, and RX buffer depth.
 ***************************************************************************/
#include "cros_lat.h"

#include "cros_bt_log.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "string.h"

typedef struct {
  uint32_t n;
  uint32_t sum_ms;
  uint16_t min_ms;
  uint16_t max_ms;
} lat_acc_t;

static lat_acc_t cap_to_send;     /* capture frame ready → encode+queue */
static lat_acc_t q_to_bt;         /* extra: app queue → l2cap_send_data */
static lat_acc_t bt_to_txdone;    /* extra: l2cap_send → TX_HANDLED */
static lat_acc_t cmd_q_to_done;   /* cmd: tws_ctrl submit → tx_done */
static lat_acc_t recv_to_put;     /* datarecv → pcmbuff_put */
static lat_acc_t rx_buf_ms;       /* pcmbuff depth at put (intentional jitter) */
static lat_acc_t play_buf_ms;     /* pcmbuff depth after successful get */

static uint32_t t_cap;
static uint8_t have_cap;
static uint32_t t_send_q;
static uint8_t have_send_q;
static uint32_t t_extra_bt;
static uint8_t have_extra_bt;
static uint32_t t_cmd_q;
static uint8_t have_cmd_q;
static uint32_t t_recv;

static uint32_t n_extra_send;
static uint32_t n_cmd_send;
static uint32_t n_underrun;
static uint32_t n_tx_fail;

static void acc_add(lat_acc_t *a, uint32_t ms) {
  if (ms > 60000u) {
    ms = 60000u; /* clamp absurd wrap / stall */
  }
  if (a->n == 0) {
    a->min_ms = (uint16_t)ms;
    a->max_ms = (uint16_t)ms;
  } else {
    if (ms < a->min_ms) {
      a->min_ms = (uint16_t)ms;
    }
    if (ms > a->max_ms) {
      a->max_ms = (uint16_t)ms;
    }
  }
  a->n++;
  a->sum_ms += ms;
}

static uint32_t acc_avg(const lat_acc_t *a) {
  if (a->n == 0) {
    return 0;
  }
  return a->sum_ms / a->n;
}

static void acc_log(const char *name, const lat_acc_t *a) {
  if (a->n == 0) {
    CROS_LOG(0, "[cros_lat] %s n=0", name);
    return;
  }
  CROS_LOG(0, "[cros_lat] %s n=%u avg=%ums min=%u max=%u", name, (unsigned)a->n,
        (unsigned)acc_avg(a), (unsigned)a->min_ms, (unsigned)a->max_ms);
}

void cros_lat_reset(void) {
  memset(&cap_to_send, 0, sizeof(cap_to_send));
  memset(&q_to_bt, 0, sizeof(q_to_bt));
  memset(&bt_to_txdone, 0, sizeof(bt_to_txdone));
  memset(&cmd_q_to_done, 0, sizeof(cmd_q_to_done));
  memset(&recv_to_put, 0, sizeof(recv_to_put));
  memset(&rx_buf_ms, 0, sizeof(rx_buf_ms));
  memset(&play_buf_ms, 0, sizeof(play_buf_ms));
  have_cap = have_send_q = have_extra_bt = have_cmd_q = 0;
  n_extra_send = n_cmd_send = n_underrun = n_tx_fail = 0;
}

void cros_lat_dump(const char *where) {
  CROS_LOG(0, "[cros_lat] DUMP @%s extra_tx=%u cmd_tx=%u underrun=%u tx_fail=%u",
        where ? where : "?", (unsigned)n_extra_send, (unsigned)n_cmd_send,
        (unsigned)n_underrun, (unsigned)n_tx_fail);
  acc_log("cap→send", &cap_to_send);
  acc_log("extra q→bt", &q_to_bt);
  acc_log("extra bt→TX_HANDLED", &bt_to_txdone);
  acc_log("cmd q→done", &cmd_q_to_done);
  acc_log("recv→put", &recv_to_put);
  acc_log("rx_buf@put", &rx_buf_ms);
  acc_log("play_buf@get", &play_buf_ms);
}

void cros_lat_note_cap_done(void) {
  t_cap = GET_CURRENT_TICKS();
  have_cap = 1;
}

void cros_lat_note_send_begin(int on_extra) {
  uint32_t now = GET_CURRENT_TICKS();
  if (have_cap) {
    acc_add(&cap_to_send, TICKS_TO_MS(now - t_cap));
    have_cap = 0;
  }
  if (on_extra) {
    n_extra_send++;
  } else {
    n_cmd_send++;
    t_cmd_q = now;
    have_cmd_q = 1;
  }
}

void cros_lat_note_extra_queued(void) {
  t_send_q = GET_CURRENT_TICKS();
  have_send_q = 1;
}

void cros_lat_note_extra_bt_sent(int ok) {
  uint32_t now = GET_CURRENT_TICKS();
  if (have_send_q) {
    acc_add(&q_to_bt, TICKS_TO_MS(now - t_send_q));
    have_send_q = 0;
  }
  if (ok) {
    t_extra_bt = now;
    have_extra_bt = 1;
  } else {
    n_tx_fail++;
    have_extra_bt = 0;
  }
}

void cros_lat_note_extra_tx_handled(void) {
  uint32_t now = GET_CURRENT_TICKS();
  if (have_extra_bt) {
    acc_add(&bt_to_txdone, TICKS_TO_MS(now - t_extra_bt));
    have_extra_bt = 0;
  }
}

void cros_lat_note_cmd_tx_done(void) {
  uint32_t now = GET_CURRENT_TICKS();
  if (have_cmd_q) {
    acc_add(&cmd_q_to_done, TICKS_TO_MS(now - t_cmd_q));
    have_cmd_q = 0;
  }
}

void cros_lat_note_recv_begin(void) { t_recv = GET_CURRENT_TICKS(); }

void cros_lat_note_recv_put(uint32_t pcmbuff_bytes_before_put) {
  uint32_t now = GET_CURRENT_TICKS();
  /* 16 kHz × 2 B = 32 B/ms */
  uint32_t buf_ms = pcmbuff_bytes_before_put / 32u;
  acc_add(&recv_to_put, TICKS_TO_MS(now - t_recv));
  acc_add(&rx_buf_ms, buf_ms);
}

void cros_lat_note_play_ok(uint32_t pcmbuff_bytes_after_get) {
  acc_add(&play_buf_ms, pcmbuff_bytes_after_get / 32u);
}

void cros_lat_note_underrun(void) { n_underrun++; }
