/***************************************************************************
 * BESAUD extra L2CAP — deferred create on CROS activate.
 *
 * v0.3.16 — quiet TOTA/SPP while extra media runs (logging killed the link).
 *  READY on PING|PONG; deep jitter; defer/single-ping coexist knobs.
 ***************************************************************************/
#include "cros_besaud_extra.h"

#include "cmsis_os.h"
#include "cros_bt_log.h"
#include "hal_trace.h"
#include "string.h"

#ifndef CROS_EXTRA_L2CAP
#define CROS_EXTRA_L2CAP 0
#endif

#if CROS_EXTRA_L2CAP
#include "app_tws_besaud.h"
#include "app_tws_ibrt.h"
#include "besaud_api.h"
#include "co_ppbuff.h"
#include "l2cap_i.h"
#include "me_api.h"

extern int app_bt_start_custom_function_in_bt_thread(uint32_t param0,
                                                     uint32_t param1,
                                                     uint32_t funcPtr);
extern bool app_tws_ibrt_tws_link_connected(void);
extern void cros_tws_on_peer_audio(uint8_t *data, uint16_t len);

#define CROS_EXTRA_TX_MAX 512
#define CROS_EXTRA_DEFER_MS 2000 /* let cmd audio settle before OPEN */
#define CROS_EXTRA_CREATE_RETRY_MS 1000
#define CROS_EXTRA_CREATE_MAX_TRIES 10
#define CROS_EXTRA_PING_MAGIC 0xC0
#define CROS_EXTRA_PONG_MAGIC 0xC1

static volatile uint32_t extra_handle;
static volatile uint8_t extra_open;
static volatile uint8_t peer_ready;
static volatile uint8_t create_issued;
static volatile uint8_t tx_busy;
static uint8_t create_tries;
static uint8_t tx_scratch[CROS_EXTRA_TX_MAX];
static uint16_t tx_scratch_len;
static uint32_t tx_ok;
static uint32_t tx_fail;
static uint32_t rx_ok;
static uint32_t rx_ping;
static uint32_t rx_pong;
static uint32_t ping_tx;

static void cros_extra_defer(void const *arg);
osTimerDef(CROS_EXTRA_DEFER, cros_extra_defer);
static osTimerId cros_extra_defer_id;

static void cros_extra_create_bt(void *a, void *b);
static void cros_extra_send_ping_bt(void *a, void *b);
static void cros_extra_send_pong_bt(void *a, void *b);

static void cros_extra_schedule_create(uint32_t delay_ms) {
  if (!cros_extra_defer_id) {
    cros_extra_defer_id =
        osTimerCreate(osTimer(CROS_EXTRA_DEFER), osTimerOnce, NULL);
  }
  if (cros_extra_defer_id) {
    create_issued = 1;
    osTimerStart(cros_extra_defer_id, delay_ms);
  } else {
    create_issued = 1;
    app_bt_start_custom_function_in_bt_thread(0, 0,
                                              (uint32_t)cros_extra_create_bt);
  }
}

static int cros_extra_notify(enum l2cap_event_enum event, uint32 l2cap_handle,
                             void *pdata, uint8 reason) {
  (void)pdata;
  (void)reason;
  switch (event) {
  case L2CAP_CHANNEL_OPENED:
  case L2CAP_CHANNEL_NEW_OPENED:
    extra_handle = l2cap_handle;
    extra_open = 1;
    peer_ready = 0;
    tx_busy = 0;
    create_tries = 0;
    /* One PING only — 0.3.9 2s retry storm contended with cmd audio. */
    CROS_LOG(2, "[cros_extra] OPEN handle=0x%08x — single ping (no retry storm)",
          (unsigned)l2cap_handle);
    app_bt_start_custom_function_in_bt_thread(0, 0,
                                              (uint32_t)cros_extra_send_ping_bt);
    break;
  case L2CAP_CHANNEL_TX_HANDLED:
    tx_busy = 0;
    break;
  case L2CAP_CHANNEL_CLOSED:
    CROS_LOG(1, "[cros_extra] CLOSED handle=0x%08x", (unsigned)l2cap_handle);
    if (extra_handle == l2cap_handle) {
      extra_handle = 0;
      extra_open = 0;
      peer_ready = 0;
      tx_busy = 0;
      create_issued = 0;
      cros_bt_log_set_quiet(0);
    }
    break;
  default:
    break;
  }
  return 0;
}

static void cros_extra_datarecv(uint32 l2cap_handle, struct pp_buff *ppb) {
  (void)l2cap_handle;
  if (!ppb || !ppb->data || ppb->len == 0) {
    return;
  }
  if (ppb->data[0] == CROS_EXTRA_PING_MAGIC) {
    rx_ping++;
    CROS_LOG(2, "[cros_extra] PING rx len=%u count=%u — pong",
          (unsigned)ppb->len, (unsigned)rx_ping);
    app_bt_start_custom_function_in_bt_thread(0, 0,
                                              (uint32_t)cros_extra_send_pong_bt);
    /* PING proves peer can reach us on extra. TX (poor) often only sees
     * PING, never PONG — 0.3.13 LEFT got READY from pong while RIGHT kept
     * cmd audio. Treat PING as READY so audio can switch to extra. */
    if (!peer_ready) {
      peer_ready = 1;
      cros_bt_log_set_quiet(1);
      CROS_LOG(1, "[cros_extra] peer READY (ping=%u) — switch audio to extra",
            (unsigned)rx_ping);
    }
    return;
  }
  if (ppb->data[0] == CROS_EXTRA_PONG_MAGIC) {
    rx_pong++;
    if (!peer_ready) {
      peer_ready = 1;
      cros_bt_log_set_quiet(1);
      CROS_LOG(1, "[cros_extra] peer READY (pong=%u) — switch audio to extra",
            (unsigned)rx_pong);
    }
    return;
  }
  if (!peer_ready) {
    peer_ready = 1;
    cros_bt_log_set_quiet(1);
    CROS_LOG(0, "[cros_extra] peer READY (audio rx) — switch audio to extra");
  }
  cros_tws_on_peer_audio(ppb->data, (uint16_t)ppb->len);
  rx_ok++;
  if ((rx_ok & 0x7F) == 0) {
    CROS_LOG_STAT(0, "[cros_extra] audio_rx=%u ping_rx=%u pong_rx=%u",
          (unsigned)rx_ok, (unsigned)rx_ping, (unsigned)rx_pong);
  }
}

static void *cros_extra_peer_addr(void) {
  btif_remote_device_t *dev;
  void *bd;
  ibrt_ctrl_t *ctx;

  /* 1) Stock BESAUD peer — works on some roles, NULL on phone-master (0.3.12). */
  dev = btif_besaud_get_peer_device();
  if (dev) {
    bd = btif_me_get_remote_device_bdaddr(dev);
    if (bd) {
      CROS_LOG(2, "[cros_extra] peer via besaud_get_peer_device");
      return bd;
    }
  }

  /* 2) Live TWS remdev from IBRT ctrl — reliable on master + slave. */
  ctx = app_tws_ibrt_get_bt_ctrl_ctx();
  if (ctx && ctx->p_tws_remote_dev) {
    bd = btif_me_get_remote_device_bdaddr(ctx->p_tws_remote_dev);
    if (bd) {
      CROS_LOG(2, "[cros_extra] peer via p_tws_remote_dev");
      return bd;
    }
  }

  /* 3) TWS ACL handle → remdev. */
  if (ctx && ctx->tws_conhandle) {
    dev = btif_me_get_remote_device_by_handle(ctx->tws_conhandle);
    if (dev) {
      bd = btif_me_get_remote_device_bdaddr(dev);
      if (bd) {
        CROS_LOG(2, "[cros_extra] peer via tws_conhandle 0x%04x",
              (unsigned)ctx->tws_conhandle);
        return bd;
      }
    }
  }

  return NULL;
}

static void cros_extra_create_bt(void *a, void *b) {
  void *remote;
  (void)a;
  (void)b;
  if (extra_open) {
    create_issued = 0;
    return;
  }
  remote = cros_extra_peer_addr();
  if (!remote) {
    create_tries++;
    CROS_LOG(0, "[cros_extra] create skipped — no peer (try %u/%u)",
          (unsigned)create_tries, (unsigned)CROS_EXTRA_CREATE_MAX_TRIES);
    if (create_tries < CROS_EXTRA_CREATE_MAX_TRIES) {
      cros_extra_schedule_create(CROS_EXTRA_CREATE_RETRY_MS);
    } else {
      create_issued = 0;
      CROS_LOG(0, "[cros_extra] create gave up — cmd-only this session");
    }
    return;
  }
  if (!tws_besaud_is_connected() && !app_tws_ibrt_tws_link_connected()) {
    create_tries++;
    CROS_LOG(0, "[cros_extra] create skipped — TWS/BESAUD down (try %u/%u)",
          (unsigned)create_tries, (unsigned)CROS_EXTRA_CREATE_MAX_TRIES);
    if (create_tries < CROS_EXTRA_CREATE_MAX_TRIES) {
      cros_extra_schedule_create(CROS_EXTRA_CREATE_RETRY_MS);
    } else {
      create_issued = 0;
    }
    return;
  }
  CROS_LOG(0, "[cros_extra] CREATE 0x0b0e (coexist, after settle)");
  l2cap_create_besaud_extra_channel(remote, L2CAP_BESAUD_EXTRA_CHAN_ID,
                                    cros_extra_notify, cros_extra_datarecv);
}

static void cros_extra_send_bt(void *a, void *b) {
  int8_t ret;
  (void)a;
  (void)b;
  if (!extra_open || !extra_handle || tx_scratch_len == 0) {
    tx_busy = 0;
    return;
  }
  ret = l2cap_send_data(extra_handle, tx_scratch, tx_scratch_len, NULL);
  if (ret != 0) {
    tx_busy = 0;
    tx_fail++;
  } else {
    tx_ok++;
    if ((tx_ok & 0x3F) == 0) {
      CROS_LOG_STAT(0, "[cros_extra] audio_tx=%u fail=%u peer_ready=%u",
            (unsigned)tx_ok, (unsigned)tx_fail, (unsigned)peer_ready);
    }
  }
}

static void cros_extra_send_ping_bt(void *a, void *b) {
  static const uint8_t ping[8] = {CROS_EXTRA_PING_MAGIC, 0x52, 0x4f, 0x53,
                                  0x50, 0x49, 0x4e, 0x47};
  (void)a;
  (void)b;
  if (!extra_open || !extra_handle || peer_ready) {
    return;
  }
  if (l2cap_send_data(extra_handle, (uint8_t *)ping, sizeof(ping), NULL) != 0) {
    CROS_LOG(0, "[cros_extra] ping send fail");
    return;
  }
  ping_tx++;
  CROS_LOG(2, "[cros_extra] PING tx=%u (single; audio stays on cmd until PONG)",
        (unsigned)ping_tx);
}

static void cros_extra_send_pong_bt(void *a, void *b) {
  static const uint8_t pong[8] = {CROS_EXTRA_PONG_MAGIC, 0x52, 0x4f, 0x53,
                                  0x50, 0x4f, 0x4e, 0x47};
  (void)a;
  (void)b;
  if (!extra_open || !extra_handle) {
    return;
  }
  if (l2cap_send_data(extra_handle, (uint8_t *)pong, sizeof(pong), NULL) != 0) {
    CROS_LOG(0, "[cros_extra] pong send fail");
  }
}

static void cros_extra_defer(void const *arg) {
  (void)arg;
  if (extra_open || !create_issued) {
    return;
  }
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_extra_create_bt);
}
#endif /* CROS_EXTRA_L2CAP */

void cros_besaud_extra_init(void) {
#if CROS_EXTRA_L2CAP
  extra_handle = 0;
  extra_open = 0;
  peer_ready = 0;
  create_issued = 0;
  create_tries = 0;
  tx_busy = 0;
  tx_scratch_len = 0;
  tx_ok = tx_fail = rx_ok = rx_ping = rx_pong = ping_tx = 0;
  if (!cros_extra_defer_id) {
    cros_extra_defer_id =
        osTimerCreate(osTimer(CROS_EXTRA_DEFER), osTimerOnce, NULL);
  }
  CROS_LOG(0, "[cros_extra] init (v0.3.16 quiet SPP on extra; defer %dms)",
        CROS_EXTRA_DEFER_MS);
#endif
}

void cros_besaud_extra_ensure(void) {
#if CROS_EXTRA_L2CAP
  if (extra_open || create_issued) {
    return;
  }
  create_tries = 0;
  CROS_LOG(1, "[cros_extra] schedule create in %dms", CROS_EXTRA_DEFER_MS);
  cros_extra_schedule_create(CROS_EXTRA_DEFER_MS);
#else
  (void)0;
#endif
}

void cros_besaud_extra_on_besaud_down(void) {
#if CROS_EXTRA_L2CAP
  if (cros_extra_defer_id) {
    osTimerStop(cros_extra_defer_id);
  }
  extra_handle = 0;
  extra_open = 0;
  peer_ready = 0;
  create_issued = 0;
  create_tries = 0;
  tx_busy = 0;
  cros_bt_log_set_quiet(0);
#endif
}

bool cros_besaud_extra_is_open(void) {
#if CROS_EXTRA_L2CAP
  return extra_open != 0;
#else
  return false;
#endif
}

bool cros_besaud_extra_peer_ready(void) {
#if CROS_EXTRA_L2CAP
  return peer_ready != 0;
#else
  return false;
#endif
}

bool cros_besaud_extra_tx_busy(void) {
#if CROS_EXTRA_L2CAP
  return tx_busy != 0;
#else
  return false;
#endif
}

void cros_besaud_extra_force_clear_pending(void) {
#if CROS_EXTRA_L2CAP
  tx_busy = 0;
#endif
}

int cros_besaud_extra_send(const uint8_t *data, uint16_t len) {
#if CROS_EXTRA_L2CAP
  if (!extra_open || !data || len == 0 || len > CROS_EXTRA_TX_MAX) {
    return -1;
  }
  if (tx_busy) {
    return -2;
  }
  memcpy(tx_scratch, data, len);
  tx_scratch_len = len;
  tx_busy = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_extra_send_bt);
  return 0;
#else
  (void)data;
  (void)len;
  return -1;
#endif
}
