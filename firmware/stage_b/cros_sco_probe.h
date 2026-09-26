/***************************************************************************
 * v0.3.37 — SCO/eSCO bud↔bud probe (latency chase §K).
 *
 * CROS_SCO_ALONE=1: skip extra L2CAP; settle from CROS enable; leave SCO up
 * until disable (prove peer SCO without extra coexistence).
 *
 * With extra (ALONE=0): READY settle → open → auto-close ~300 ms after OPENED
 * (0.3.36 — SCO+extra wedged). No SCO audio yet.
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
