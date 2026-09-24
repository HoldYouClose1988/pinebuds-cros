/***************************************************************************
 * CROS latency / path probe (B+H) — measure hops; dump on DISABLE.
 *
 * Does not change media timing. Safe to leave on in measurement builds.
 ***************************************************************************/
#ifndef CROS_LAT_H
#define CROS_LAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cros_lat_reset(void);
void cros_lat_dump(const char *where);

/* TX (poor) */
void cros_lat_note_cap_done(void);
void cros_lat_note_send_begin(int on_extra);
void cros_lat_note_extra_queued(void);
void cros_lat_note_extra_bt_sent(int ok);
void cros_lat_note_extra_tx_handled(void);
void cros_lat_note_cmd_tx_done(void);

/* RX (good) */
void cros_lat_note_recv_begin(void);
void cros_lat_note_recv_put(uint32_t pcmbuff_bytes_before_put);
void cros_lat_note_play_ok(uint32_t pcmbuff_bytes_after_get);
void cros_lat_note_underrun(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_LAT_H */
