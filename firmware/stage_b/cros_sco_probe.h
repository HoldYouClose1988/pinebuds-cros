/***************************************************************************
 * v0.3.30 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * Register + open SCO to TWS peer; log OPEN/CLOSED (+ stack BTEVENT tee).
 * No mic/speaker on SCO yet. Extra L2CAP CROS audio stays on the baseline.
 *
 * v0.3.30: open on peer READY (PING/PONG), not only the 1.5 s timer.
 * Optional CROS_SCO_SLAVE_OPEN=1 so slave also issues open_link.
 *
 * Prefer phone disconnected (mobile_conhandle==0) for clean peer probe.
 * Phone SCO reference: android/cros-log "Phone SCO on" button.
 * Build with CROS_SCO_PROBE=0 to disable.
 ***************************************************************************/
#ifndef CROS_SCO_PROBE_H
#define CROS_SCO_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

void cros_sco_probe_init(void);
/* Schedule deferred open while CROS is enabling. Idempotent. */
void cros_sco_probe_on_cros_enable(void);
/* Close/unregister on CROS disable. */
void cros_sco_probe_on_cros_disable(void);
/* Peer READY on extra — open SCO now (cancels fallback timer). */
void cros_sco_probe_on_peer_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
