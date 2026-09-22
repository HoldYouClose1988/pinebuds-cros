/***************************************************************************
 * CROS Bluetooth log tee — ring buffer → TOTA SPP (tota_printf).
 *
 * Always mirrors to UART TRACE. When TEST_OVER_THE_AIR_ENANBLED (TOTA=1),
 * also queues lines for phone capture. Never call SPP from ISR context;
 * enqueue only, flush from the timer.
 ***************************************************************************/
#ifndef CROS_BT_LOG_H
#define CROS_BT_LOG_H

#include "hal_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

void cros_bt_log_init(void);

#ifdef TEST_OVER_THE_AIR_ENANBLED
void cros_bt_logf(const char *fmt, ...);
#define CROS_LOG(n, fmt, ...)                                                  \
  do {                                                                         \
    TRACE((n), (fmt), ##__VA_ARGS__);                                          \
    cros_bt_logf((fmt), ##__VA_ARGS__);                                        \
  } while (0)
#else
#define cros_bt_logf(...) ((void)0)
#define CROS_LOG TRACE
#endif

#ifdef __cplusplus
}
#endif

#endif /* CROS_BT_LOG_H */
