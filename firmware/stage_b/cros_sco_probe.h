/***************************************************************************
 * v0.3.36 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * No SCO on CROS enable (early register crashed RIGHT-master+TX in 0.3.31).
 * On peer READY: settle (0.5 s / 1.5 s poor), then register+open.
 * 12 s late fallback if READY never arrives.
 *
 * OPENED proved with CROS_SCO_SLAVE_OPEN=1 (0.3.35 ear). Leaving peer SCO up
 * under extra L2CAP media + mobile ACL wedged the buds — auto-close ~300 ms
 * after OPENED (proof only; no SCO audio yet).
 ***************************************************************************/
#ifndef CROS_SCO_PROBE_H
#define CROS_SCO_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

void cros_sco_probe_init(void);
/* Arm probe only — no sco_init/register until READY settle. */
void cros_sco_probe_on_cros_enable(void);
void cros_sco_probe_on_cros_disable(void);
void cros_sco_probe_on_peer_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_SCO_PROBE_H */
