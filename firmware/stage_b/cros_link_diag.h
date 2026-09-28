/***************************************************************************
 * TWS ACL link diagnostics (read-only).
 *
 * Logs vendor private-poll settings vs live controller registers so we can
 * see which poll tier is in effect with / without peer SCO. Does not change
 * link policy. See docs/archive/tws-link-tuning.md.
 ***************************************************************************/
#ifndef CROS_LINK_DIAG_H
#define CROS_LINK_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/* where: short tag e.g. "enable", "sco-opened", "disable" */
void cros_link_diag_log(const char *where);

#ifdef __cplusplus
}
#endif

#endif /* CROS_LINK_DIAG_H */
