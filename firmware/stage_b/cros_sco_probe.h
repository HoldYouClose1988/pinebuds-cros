/***************************************************************************
 * v0.3.39 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * CROS_SCO_ALONE=1: skip extra L2CAP; settle from CROS enable; leave SCO up
 * until disable.
 *
 * CROS_SCO_MEDIA=1: on OPENED, start stock HFP CVSD voice player
 * (hfp_ibrt_sco_audio_connected) against the peer SCO handle — first
 * mic↔SCO↔speaker probe. Not asymmetric CROS yet (full-duplex trial).
 ***************************************************************************/
#ifndef CROS_SCO_PROBE_H
#define CROS_SCO_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

void cros_sco_probe_init(void);
/* Arm probe only — no sco_init/register until settle / READY. */
void cros_sco_probe_on_cros_enable(void);
void cros_sco_probe_on_cros_disable(void);
void cros_sco_probe_on_peer_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
