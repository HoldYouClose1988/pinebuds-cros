/***************************************************************************
 * v0.3.31 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * Register early on CROS enable; open on peer READY only (no 1.5 s race).
 * 12 s late fallback if READY never arrives. No SCO audio yet.
 *
 * Optional CROS_SCO_SLAVE_OPEN=1 so slave also issues open_link.
 * Phone SCO reference: android/cros-log "Phone SCO on" + HFP/BTEVENT tees.
 ***************************************************************************/
#ifndef CROS_SCO_PROBE_H
#define CROS_SCO_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

void cros_sco_probe_init(void);
/* Arm probe + early register; open waits for peer READY. */
void cros_sco_probe_on_cros_enable(void);
/* Close/unregister on CROS disable. */
void cros_sco_probe_on_cros_disable(void);
/* Peer READY on extra — open SCO now. */
void cros_sco_probe_on_peer_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
