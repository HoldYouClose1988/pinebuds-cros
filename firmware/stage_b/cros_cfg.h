/***************************************************************************
 * Runtime BiCROS knobs — phone TOTA string cmds + peer IBRT sync.
 *
 * Phone (OP_TOTA_STRING, unencrypted):
 *   "cros get"
 *   "cros set poor=right mix=-20 bass=0 treble=0"
 * Keys may be sent individually. poor=left|right|0|1; mix=-30..0 dB;
 * bass/treble=-6..+6 dB (SCO playback shelves on good ear).
 ***************************************************************************/
#ifndef CROS_CFG_H
#define CROS_CFG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cros_cfg_init(void);

/* TOTA SPP RX (patched into app_tota_data_handler). */
void cros_cfg_on_tota_string(uint8_t *param, uint32_t param_len);

/* Peer IBRT custom-cmd RX. */
void cros_cfg_on_peer(const uint8_t *data, uint16_t len);

/* Accessors used by cros_tws / sco path. */
int cros_cfg_poor_is_right(void);
int8_t cros_cfg_mix_db(void);
int8_t cros_cfg_bass_db(void);
int8_t cros_cfg_treble_db(void);

/* Soft EQ on SCO PCM (good ear). No-op if flat or poor side. */
void cros_cfg_process_sco_pcm(uint8_t *buf, uint32_t len);

/* Re-apply sidetone gain / EQ coeffs after live change. */
void cros_cfg_apply_audio(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_CFG_H */
