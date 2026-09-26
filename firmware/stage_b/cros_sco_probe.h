/***************************************************************************
 * v0.3.40 — SCO/eSCO bud↔bud CROS shape (latency chase §K).
 *
 * CROS_SCO_ALONE=1: skip extra L2CAP; settle from enable; leave SCO up.
 * CROS_SCO_MEDIA=1: on OPENED start HFP CVSD voice on peer SCO, then
 *   asymmetric mute — POOR/TX: mic ON spk OFF; GOOD/RX: mic OFF spk ON.
 * Quality later (Stability → Latency → Quality).
 ***************************************************************************/
#ifndef CROS_SCO_PROBE_H
#define CROS_SCO_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

void cros_sco_probe_init(void);
void cros_sco_probe_on_cros_enable(void);
void cros_sco_probe_on_cros_disable(void);
void cros_sco_probe_on_peer_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
