/***************************************************************************
 * Stage A: FF mic → speaker loopback for PineBuds Pro CROS bring-up.
 * Project-owned code (MIT); integrates into OpenPineBuds via patches.
 ***************************************************************************/
#ifndef CROS_LOOPBACK_H
#define CROS_LOOPBACK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void cros_loopback_init(void);
int cros_loopback_start(void);
int cros_loopback_stop(void);
int cros_loopback_toggle(void);
bool cros_loopback_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_LOOPBACK_H */
