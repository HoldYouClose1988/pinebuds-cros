/***************************************************************************
 * v0.3.46 — SCO BiCROS: split register/open + pause TOTA flush under hold.
 *
 * mSBC 16 kHz peer SCO CROS + HW codec sidetone on GOOD only (local mic →
 * speaker while SCO RX plays). Digital mic TX stays muted on good so local
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
/* HCI BTEVENT_SCO_DISCONNECT — rem6 may be NULL; peer-filtered. */
void cros_sco_on_hci_disconnect(uint8_t err, const uint8_t *rem6);

/* Re-run asymmetric mute / sidetone after live cfg change (mix / poor). */
void cros_sco_reapply_shape(void);

/* True while peer SCO is up — skip IBRT cfg sync. */
int cros_sco_cfg_hold(void);
/* True while CROS SCO armed/opening/up — pause SPP ring tee (ack-only). */
int cros_sco_log_hold(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
