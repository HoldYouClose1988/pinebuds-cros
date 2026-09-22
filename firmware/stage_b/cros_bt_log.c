/***************************************************************************
 * CROS Bluetooth log tee — see cros_bt_log.h.
 ***************************************************************************/
#include "cros_bt_log.h"

#include "cmsis_os.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

#ifdef TEST_OVER_THE_AIR_ENANBLED
#include "app_tota.h"

#define CROS_BT_LOG_LINE_MAX 120
#define CROS_BT_LOG_DEPTH 24
#define CROS_BT_LOG_FLUSH_MS 80

typedef struct {
  char line[CROS_BT_LOG_LINE_MAX];
} cros_bt_log_slot_t;

static cros_bt_log_slot_t ring[CROS_BT_LOG_DEPTH];
static volatile uint8_t head;
static volatile uint8_t tail;
static volatile uint8_t dropped;
static uint8_t inited;

static void cros_bt_log_flush(void const *arg);
osTimerDef(CROS_BT_LOG_FLUSH, cros_bt_log_flush);
static osTimerId flush_id;

static void cros_bt_log_flush(void const *arg) {
  (void)arg;
  uint8_t drops = dropped;
  if (drops) {
    dropped = 0;
    tota_printf("[cros_log] dropped=%u", (unsigned)drops);
  }
  while (tail != head) {
    uint8_t i = tail;
    tota_printf("%s", ring[i].line);
    tail = (uint8_t)((i + 1u) % CROS_BT_LOG_DEPTH);
  }
}

void cros_bt_log_init(void) {
  if (inited) {
    return;
  }
  head = tail = dropped = 0;
  flush_id = osTimerCreate(osTimer(CROS_BT_LOG_FLUSH), osTimerPeriodic, NULL);
  if (flush_id) {
    osTimerStart(flush_id, CROS_BT_LOG_FLUSH_MS);
  }
  inited = 1;
  TRACE(0, "[cros_log] init (TOTA tee ON)");
}

void cros_bt_logf(const char *fmt, ...) {
  char buf[CROS_BT_LOG_LINE_MAX];
  va_list ap;
  if (!inited) {
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

#endif
