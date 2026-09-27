/***************************************************************************
 * CROS Bluetooth log tee — see cros_bt_log.h.
 *
 * Timer only schedules work. Actual tota_printf runs on the BT thread,
 * capped per tick, and only when the TOTA SPP path is up. Never call
 * tota_printf (osSemaphoreWait forever) from a general-purpose OS timer.
 *
 * v0.3.46: while peer SCO is up, throttle TOTA flush (don't block BT).
 * v0.3.47: ack slot for Apply/Get under load.
 * v0.3.49: hold = sco_up only — never mute after the SCO pipe is down;
 *   under SCO, throttle to 1 line/tick (do not drain/discard the ring).
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
/* While peer SCO is up, still tee — but at most one SPP line per tick. */
#define CROS_BT_LOG_FLUSH_MAX_SCO 1

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
/* Single Apply/Get confirmation — flushed first under load. */
static char ack_line[CROS_BT_LOG_LINE_MAX];
static volatile uint8_t ack_pending;

static void cros_bt_log_timer(void const *arg);
static void cros_bt_log_flush_bt(void *a, void *b);
static void cros_bt_log_kick_flush(void);

osTimerDef(CROS_BT_LOG_FLUSH, cros_bt_log_timer);
static osTimerId flush_id;

static void cros_bt_log_kick_flush(void) {
  if (!inited || flush_pending) {
    return;
  }
  flush_pending = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_bt_log_flush_bt);
}

static void cros_bt_log_flush_bt(void *a, void *b) {
  uint8_t sent = 0;
  uint8_t max_lines;
  uint8_t drops;
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

  /* Ack first (Apply/Get / DISABLE) — one line even under SCO. */
  if (ack_pending) {
    ack_pending = 0;
    tota_printf("%s", ack_line);
    sent++;
  }

  /* SCO up: throttle, never discard. SCO down: normal multi-line flush. */
  max_lines = cros_sco_cfg_hold() ? CROS_BT_LOG_FLUSH_MAX_SCO
                                  : CROS_BT_LOG_FLUSH_MAX;

  drops = dropped;
  if (drops && !quiet && sent < max_lines) {
    dropped = 0;
    tota_printf("[cros_log] dropped=%u", (unsigned)drops);
    sent++;
  } else if (drops && quiet) {
    dropped = 0;
  }

  while (sent < max_lines && tail != head) {
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
  if (ack_pending || tail != head || dropped != 0) {
    cros_bt_log_kick_flush();
  }
}

void cros_bt_log_init(void) {
  if (inited) {
    return;
  }
  head = tail = dropped = flush_pending = 0;
  quiet = 0;
  ack_pending = 0;
  ack_line[0] = '\0';
  flush_id = osTimerCreate(osTimer(CROS_BT_LOG_FLUSH), osTimerPeriodic, NULL);
  if (flush_id) {
    osTimerStart(flush_id, CROS_BT_LOG_FLUSH_MS);
  }
  inited = 1;
  TRACE(0, "[cros_log] init (TOTA tee ON, flush max=%u, sco_throttle=%u)",
        (unsigned)CROS_BT_LOG_FLUSH_MAX, (unsigned)CROS_BT_LOG_FLUSH_MAX_SCO);
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
  /* Always enqueue — SCO-up only throttles flush rate, never drops on hold. */
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
  if (!inited) {
    return;
  }
  va_start(ap, fmt);
  vsnprintf(ack_line, sizeof(ack_line), fmt, ap);
  va_end(ap);
  ack_line[sizeof(ack_line) - 1] = '\0';
  ack_pending = 1;
  cros_bt_log_kick_flush();
}

#else /* !TEST_OVER_THE_AIR_ENANBLED */

void cros_bt_log_init(void) {
  TRACE(0, "[cros_log] init (TOTA tee OFF — build with TOTA=1)");
}

void cros_bt_log_set_quiet(int on) { (void)on; }

int cros_bt_log_is_quiet(void) { return 0; }

#endif
