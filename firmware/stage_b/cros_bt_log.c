/***************************************************************************
 * CROS Bluetooth log tee — see cros_bt_log.h.
 *
 * Timer only schedules work. Actual tota_printf runs on the BT thread,
 * capped per tick, and only when the TOTA SPP path is up.
 *
 * v0.3.51: under SCO — no bulk ring flush (keeps taps alive). Curated
 * milestones use an ack *queue* (depth 8), flushed 1 line/tick so ENABLE /
 * OPENED / shape / DISABLE reach the phone without SPP storms.
 ***************************************************************************/
#include "cros_bt_log.h"

#include "cmsis_os.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

#ifdef TEST_OVER_THE_AIR_ENANBLED
#include "app_tota.h"

extern int app_bt_start_custom_function_in_bt_thread(uint32_t param0,
                                                     uint32_t param1,
                                                     uint32_t funcPtr);
extern int cros_sco_log_hold(void);

#define CROS_BT_LOG_LINE_MAX 120
#define CROS_BT_LOG_DEPTH 24
#define CROS_BT_ACK_DEPTH 8
#define CROS_BT_LOG_FLUSH_MS 80
#define CROS_BT_LOG_FLUSH_MAX 2
/* Under SCO: at most one ack line per tick (never the bulk ring). */
#define CROS_BT_ACK_FLUSH_MAX 1

typedef struct {
  char line[CROS_BT_LOG_LINE_MAX];
} cros_bt_log_slot_t;

static cros_bt_log_slot_t ring[CROS_BT_LOG_DEPTH];
static volatile uint8_t head;
static volatile uint8_t tail;
static volatile uint8_t dropped;
static volatile uint8_t flush_pending;
static volatile uint8_t quiet;
static uint8_t inited;

static cros_bt_log_slot_t ack_ring[CROS_BT_ACK_DEPTH];
static volatile uint8_t ack_head;
static volatile uint8_t ack_tail;
static volatile uint8_t ack_dropped;

static void cros_bt_log_timer(void const *arg);
static void cros_bt_log_flush_bt(void *a, void *b);
static void cros_bt_log_kick_flush(void);

osTimerDef(CROS_BT_LOG_FLUSH, cros_bt_log_timer);
static osTimerId flush_id;

static void cros_bt_log_drain_ring(void) {
  tail = head;
  dropped = 0;
}

static int cros_bt_ack_empty(void) { return ack_tail == ack_head; }

static void cros_bt_log_kick_flush(void) {
  if (!inited || flush_pending) {
    return;
  }
  flush_pending = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_bt_log_flush_bt);
}

static uint8_t cros_bt_flush_acks(uint8_t max_lines) {
  uint8_t sent = 0;
  while (sent < max_lines && ack_tail != ack_head) {
    uint8_t i = ack_tail;
    tota_printf("%s", ack_ring[i].line);
    ack_tail = (uint8_t)((i + 1u) % CROS_BT_ACK_DEPTH);
    sent++;
  }
  return sent;
}

static void cros_bt_log_flush_bt(void *a, void *b) {
  uint8_t sent = 0;
  uint8_t drops;
  (void)a;
  (void)b;

  if (!app_is_in_tota_mode()) {
    flush_pending = 0;
    return;
  }

  if (cros_sco_log_hold()) {
    /* Milestones only — one ack/tick. Drop bulk ring (UART has TRACE). */
    (void)cros_bt_flush_acks(CROS_BT_ACK_FLUSH_MAX);
    cros_bt_log_drain_ring();
    flush_pending = 0;
    return;
  }

  sent = cros_bt_flush_acks(CROS_BT_LOG_FLUSH_MAX);

  drops = dropped;
  if (drops && !quiet && sent < CROS_BT_LOG_FLUSH_MAX) {
    dropped = 0;
    tota_printf("[cros_log] dropped=%u", (unsigned)drops);
    sent++;
  } else if (drops && quiet) {
    dropped = 0;
  }
  if (ack_dropped && !quiet && sent < CROS_BT_LOG_FLUSH_MAX) {
    uint8_t ad = ack_dropped;
    ack_dropped = 0;
    tota_printf("[cros_log] ack_dropped=%u", (unsigned)ad);
    sent++;
  } else if (ack_dropped && quiet) {
    ack_dropped = 0;
  }

  while (sent < CROS_BT_LOG_FLUSH_MAX && tail != head) {
    uint8_t i = tail;
    tota_printf("%s", ring[i].line);
    tail = (uint8_t)((i + 1u) % CROS_BT_LOG_DEPTH);
    sent++;
  }

  flush_pending = 0;
}

static void cros_bt_log_timer(void const *arg) {
  (void)arg;
  if (!inited) {
    return;
  }
  if (cros_sco_log_hold()) {
    if (!cros_bt_ack_empty()) {
      cros_bt_log_kick_flush();
    } else if (tail != head || dropped) {
      cros_bt_log_drain_ring();
    }
    return;
  }
  if (!cros_bt_ack_empty() || tail != head || dropped != 0 || ack_dropped != 0) {
    cros_bt_log_kick_flush();
  }
}

void cros_bt_log_init(void) {
  if (inited) {
    return;
  }
  head = tail = dropped = flush_pending = 0;
  ack_head = ack_tail = ack_dropped = 0;
  quiet = 0;
  flush_id = osTimerCreate(osTimer(CROS_BT_LOG_FLUSH), osTimerPeriodic, NULL);
  if (flush_id) {
    osTimerStart(flush_id, CROS_BT_LOG_FLUSH_MS);
  }
  inited = 1;
  TRACE(0, "[cros_log] init (TOTA tee ON, flush max=%u, sco=ack-q%d)",
        (unsigned)CROS_BT_LOG_FLUSH_MAX, (int)CROS_BT_ACK_DEPTH);
}

void cros_bt_log_set_quiet(int on) {
  uint8_t was = quiet;
  quiet = on ? 1 : 0;
  if (was != quiet) {
    TRACE(0, "[cros_log] quiet=%u (extra media SPP throttle)", (unsigned)quiet);
    if (inited) {
      cros_bt_logf("[cros_log] quiet=%u", (unsigned)quiet);
    }
  }
}

int cros_bt_log_is_quiet(void) { return quiet ? 1 : 0; }

void cros_bt_logf(const char *fmt, ...) {
  char buf[CROS_BT_LOG_LINE_MAX];
  va_list ap;
  if (!inited) {
    return;
  }
  if (cros_sco_log_hold()) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';

  {
    uint8_t next = (uint8_t)((head + 1u) % CROS_BT_LOG_DEPTH);
    if (next == tail) {
      dropped++;
      return;
    }
    strncpy(ring[head].line, buf, CROS_BT_LOG_LINE_MAX - 1);
    ring[head].line[CROS_BT_LOG_LINE_MAX - 1] = '\0';
    head = next;
  }
}

void cros_bt_logf_stat(const char *fmt, ...) {
  char buf[CROS_BT_LOG_LINE_MAX];
  va_list ap;
  if (!inited || quiet) {
    return;
  }
  if (cros_sco_log_hold()) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';

  {
    uint8_t next = (uint8_t)((head + 1u) % CROS_BT_LOG_DEPTH);
    if (next == tail) {
      dropped++;
      return;
    }
    strncpy(ring[head].line, buf, CROS_BT_LOG_LINE_MAX - 1);
    ring[head].line[CROS_BT_LOG_LINE_MAX - 1] = '\0';
    head = next;
  }
}

void cros_bt_logf_ack(const char *fmt, ...) {
  va_list ap;
  uint8_t next;
  if (!inited) {
    return;
  }
  next = (uint8_t)((ack_head + 1u) % CROS_BT_ACK_DEPTH);
  if (next == ack_tail) {
    ack_dropped++;
    /* Overwrite oldest so newest milestones still land. */
    ack_tail = (uint8_t)((ack_tail + 1u) % CROS_BT_ACK_DEPTH);
  }
  va_start(ap, fmt);
  vsnprintf(ack_ring[ack_head].line, CROS_BT_LOG_LINE_MAX, fmt, ap);
  va_end(ap);
  ack_ring[ack_head].line[CROS_BT_LOG_LINE_MAX - 1] = '\0';
  ack_head = next;
  cros_bt_log_kick_flush();
}

#else /* !TEST_OVER_THE_AIR_ENANBLED */

void cros_bt_log_init(void) {
  TRACE(0, "[cros_log] init (TOTA tee OFF — build with TOTA=1)");
}

void cros_bt_log_set_quiet(int on) { (void)on; }

int cros_bt_log_is_quiet(void) { return 0; }

#endif
