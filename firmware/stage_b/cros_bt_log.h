/***************************************************************************
 * CROS Bluetooth log tee — ring buffer → TOTA SPP (tota_printf).
 *
 * Always mirrors to UART TRACE. When TEST_OVER_THE_AIR_ENANBLED (TOTA=1),
 * also queues lines for phone capture.
 *
 * Enqueue from any task; never send from ISR. A periodic timer only
 * schedules work onto the BT thread (same pattern as extra L2CAP TX).
 * The BT-thread flush skips if SPP is down, and sends at most a few lines
 * per tick so a slow phone drops/delays logs instead of blocking forever
 * on tota's internal osSemaphoreWait.
 *
 * Quiet mode (v0.3.16): while extra media is live, periodic stats must not
 * tee to SPP — that traffic kills the extra link. CROS_LOG always tees
 * (state transitions); CROS_LOG_STAT tees only when not quiet.
 *
 * Ack (v0.3.51): CROS_LOG_ACK queues curated milestones (ENABLE / OPENED /
 * shape / cfg / DISABLE) — flushed 1/tick under SCO. Bulk CROS_LOG stays
 * UART-only while SCO is armed (ring flush wedges taps).
 ***************************************************************************/
#ifndef CROS_BT_LOG_H
#define CROS_BT_LOG_H

#include "hal_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

void cros_bt_log_init(void);
/* When quiet!=0, CROS_LOG_STAT skips the TOTA tee (UART TRACE still runs). */
void cros_bt_log_set_quiet(int quiet);
int cros_bt_log_is_quiet(void);

#ifdef TEST_OVER_THE_AIR_ENANBLED
void cros_bt_logf(const char *fmt, ...);
void cros_bt_logf_stat(const char *fmt, ...);
/* Curated milestone — tees under SCO via ack queue (1 line/tick). */
void cros_bt_logf_ack(const char *fmt, ...);
#define CROS_LOG(n, fmt, ...)                                                  \
  do {                                                                         \
    TRACE((n), (fmt), ##__VA_ARGS__);                                          \
    cros_bt_logf((fmt), ##__VA_ARGS__);                                        \
  } while (0)
#define CROS_LOG_STAT(n, fmt, ...)                                             \
  do {                                                                         \
    TRACE((n), (fmt), ##__VA_ARGS__);                                          \
    cros_bt_logf_stat((fmt), ##__VA_ARGS__);                                   \
  } while (0)
#define CROS_LOG_ACK(n, fmt, ...)                                              \
  do {                                                                         \
    TRACE((n), (fmt), ##__VA_ARGS__);                                          \
    cros_bt_logf_ack((fmt), ##__VA_ARGS__);                                    \
  } while (0)
#else
#define cros_bt_logf(...) ((void)0)
#define cros_bt_logf_stat(...) ((void)0)
#define cros_bt_logf_ack(...) ((void)0)
#define CROS_LOG TRACE
#define CROS_LOG_STAT TRACE
#define CROS_LOG_ACK TRACE
#define cros_bt_log_set_quiet(q) ((void)(q))
#define cros_bt_log_is_quiet() (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* CROS_BT_LOG_H */
