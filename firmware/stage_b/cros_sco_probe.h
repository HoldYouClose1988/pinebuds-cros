/***************************************************************************
 * v0.3.42 — SCO CROS: mSBC 16 kHz (was CVSD 8 kHz).
 *
 * Alone + media + asymmetric mute (poor TX / good RX). Quality step after
 * stable ~140 ms latency. Tablet HFP volume does not reach peer SCO — bud
 * keys + hfp_vol bump (0.3.41) apply.
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
