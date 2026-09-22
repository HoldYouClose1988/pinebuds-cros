/***************************************************************************
 * Stage B: cross-bud CROS — poor-side FF mic → good-side speaker over TWS.
 *
 * Default poor side = RIGHT (common for this bring-up; override with
 * CROS_POOR_IS_RIGHT=0 at build time).
 *
 * Transport: raw 16 kHz mono PCM over IBRT custom cmd (no phone in path).
 * Quad-tap toggles CROS on/off and syncs the mode byte to the peer.
 ***************************************************************************/
#ifndef CROS_TWS_H
#define CROS_TWS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cros_tws_init(void);
int cros_tws_start(void);
int cros_tws_stop(void);
int cros_tws_toggle(void);
bool cros_tws_is_enabled(void);
bool cros_tws_is_poor_side(void);

/* Called from IBRT custom-cmd RX handlers (app_ibrt_customif_cmd.cpp). */
void cros_tws_on_peer_mode(uint8_t enabled);
void cros_tws_on_peer_audio(uint8_t *data, uint16_t len);
void cros_tws_on_audio_tx_done(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_TWS_H */
