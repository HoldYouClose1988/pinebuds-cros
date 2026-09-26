/***************************************************************************
 * v0.3.43 — SCO BiCROS: mix good-ear (LEFT) mic into local playback.
 *
 * mSBC 16 kHz peer SCO CROS + HW codec sidetone on GOOD only (local mic →
 * speaker while SCO RX plays). Digital mic TX stays muted on good so left
 * mic is not sent over SCO. POOR: sidetone off. SPEECH_SIDETONE=1 at build.
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
