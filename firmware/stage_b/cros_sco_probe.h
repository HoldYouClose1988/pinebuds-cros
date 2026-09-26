/***************************************************************************
 * v0.3.28 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * Smallest probe: register + open SCO to TWS peer, log OPEN/CLOSED only.
 * No mic/speaker on SCO yet. Extra L2CAP CROS audio stays on the baseline.
 *
 * Prefer phone disconnected (mobile_conhandle==0). Build with CROS_SCO_PROBE=0
 * to disable.
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

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
