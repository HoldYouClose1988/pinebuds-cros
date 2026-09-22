/***************************************************************************
 * BESAUD extra L2CAP (CID 0x0b0e) for Stage B CROS audio.
 *
 * Stock tws_besaud_* wrappers register TRACE-and-discard RX stubs — do not
 * use them. We call l2cap_create_besaud_extra_channel with our own notify /
 * datarecv. MODE sync stays on the IBRT custom-cmd path.
 ***************************************************************************/
#ifndef CROS_BESAUD_EXTRA_H
#define CROS_BESAUD_EXTRA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cros_besaud_extra_init(void);
/* Create channel once BESAUD/TWS is up (both buds). Safe to call repeatedly. */
void cros_besaud_extra_ensure(void);
void cros_besaud_extra_on_besaud_down(void);

bool cros_besaud_extra_is_open(void);

/* Queue a packet for BT-thread send. Returns 0 if accepted, <0 if busy/closed.
 * Clears pending via L2CAP_CHANNEL_TX_HANDLED (or immediate fail). */
int cros_besaud_extra_send(const uint8_t *data, uint16_t len);

/* TX-done for the pending gate (true while a send is in flight). */
bool cros_besaud_extra_tx_busy(void);
void cros_besaud_extra_force_clear_pending(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_BESAUD_EXTRA_H */
