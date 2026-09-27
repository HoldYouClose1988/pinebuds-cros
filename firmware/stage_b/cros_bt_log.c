/***************************************************************************
 * CROS Bluetooth log tee — see cros_bt_log.h.
 *
 * Timer only schedules work. Actual tota_printf runs on the BT thread,
 * capped per tick, and only when the TOTA SPP path is up. Never call
 * tota_printf (osSemaphoreWait forever) from a general-purpose OS timer.
 *
 * v0.3.46: while cros_sco_cfg_hold(), do not flush to SPP — tota_printf on
 * the BT thread during peer SCO wedges taps/Apply (ear log 221736).
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
/* Defined in cros_sco_probe.c — 0 when SCO probe is compiled out. */
extern int cros_sco_cfg_hold(void);

#define CROS_BT_LOG_LINE_MAX 120
#define CROS_BT_LOG_DEPTH 24
#define CROS_BT_LOG_FLUSH_MS 80
/* Cap per BT-thread flush so a slow phone degrades to drop/delay, not a
 * multi-line blocking chain on the BT thread. */
#define CROS_BT_LOG_FLUSH_MAX 2

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

static void cros_bt_log_timer(void const *arg);
static void cros_bt_log_flush_bt(void *a, void *b);

osTimerDef(CROS_BT_LOG_FLUSH, cros_bt_log_timer);
static osTimerId flush_id;

static void cros_bt_log_drain_ring(void) {
  tail = head;
  dropped = 0;
}

static void cros_bt_log_flush_bt(void *a, void *b) {
  (void)a;
  (void)b;

  /*
   * app_is_in_tota_mode() = SPP connected (what string TX needs).
   * is_tota_connected() is the AES handshake flag — not required for
   * OP_TOTA_STRING, which is explicitly unencrypted.
   */
  if (!app_is_in_tota_mode()) {
    flush_pending = 0;
    return;
  }

  /* Peer SCO owns the BT thread budget — never block on SPP TX here. */
  if (cros_sco_cfg_hold()) {
    cros_bt_log_drain_ring();
    flush_pending = 0;
    return;
  }

  uint8_t sent = 0;
  uint8_t drops = dropped;
  if (drops && !quiet) {
    dropped = 0;
    tota_printf("[cros_log] dropped=%u", (unsigned)drops);
    sent++;
  } else if (drops && quiet) {
    dropped = 0; /* discard count while quiet — do not SPP-spam */
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
  if (cros_sco_cfg_hold()) {
    /* Drop tee backlog while SCO hold — UART TRACE already has the lines. */
    if (tail != head || dropped) {
      cros_bt_log_drain_ring();
    }
    flush_pending = 0;
    return;
  }
  if (tail == head && dropped == 0) {
    return;
  }
  if (flush_pending) {
    return;
  }
  flush_pending = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_bt_log_flush_bt);
}

void cros_bt_log_init(void) {
  if (inited) {
    return;
  }
  head = tail = dropped = flush_pending = 0;
  quiet = 0;
  flush_id = osTimerCreate(osTimer(CROS_BT_LOG_FLUSH), osTimerPeriodic, NULL);
  if (flush_id) {
    osTimerStart(flush_id, CROS_BT_LOG_FLUSH_MS);
  }
  inited = 1;
  TRACE(0, "[cros_log] init (TOTA tee ON, flush max=%u, quiet-on-extra, "
           "hold-pause)",
        (unsigned)CROS_BT_LOG_FLUSH_MAX);
}

void cros_bt_log_set_quiet(int on) {
  uint8_t was = quiet;
  quiet = on ? 1 : 0;
  if (was != quiet) {
    TRACE(0, "[cros_log] quiet=%u (extra media SPP throttle)", (unsigned)quiet);
    /* One tee line so the phone sees the transition even as we go quiet. */
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
  /* Under SCO hold, UART-only — do not fill the SPP ring. */
  if (cros_sco_cfg_hold()) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';

  uint8_t next = (uint8_t)((head + 1u) % CROS_BT_LOG_DEPTH);
  if (next == tail) {
    dropped++;
    return;
  }
  strncpy(ring[head].line, buf, CROS_BT_LOG_LINE_MAX - 1);
  ring[head].line[CROS_BT_LOG_LINE_MAX - 1] = '\0';
  head = next;
}

void cros_bt_logf_stat(const char *fmt, ...) {
  char buf[CROS_BT_LOG_LINE_MAX];
  va_list ap;
  if (!inited || quiet) {
    return;
  }
  if (cros_sco_cfg_hold()) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';

  uint8_t next = (uint8_t)((head + 1u) % CROS_BT_LOG_DEPTH);
  if (next == tail) {
    dropped++;
    return;
  }
  strncpy(ring[head].line, buf, CROS_BT_LOG_LINE_MAX - 1);
  ring[head].line[CROS_BT_LOG_LINE_MAX - 1] = '\0';
  head = next;
}

#else /* !TEST_OVER_THE_AIR_ENANBLED */

void cros_bt_log_init(void) {
  TRACE(0, "[cros_log] init (TOTA tee OFF — build with TOTA=1)");
}

void cros_bt_log_set_quiet(int on) { (void)on; }

int cros_bt_log_is_quiet(void) { return 0; }

#endif
