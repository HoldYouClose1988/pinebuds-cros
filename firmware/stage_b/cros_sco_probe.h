/***************************************************************************
 * v0.3.32 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * No SCO on CROS enable (early register crashed RIGHT-master+TX in 0.3.31).
 * On peer READY: settle (0.5 s / 1.5 s poor), then register+open.
 * 12 s late fallback if READY never arrives. No SCO audio yet.
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
