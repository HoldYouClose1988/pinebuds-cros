/***************************************************************************
 * Runtime BiCROS knobs — phone TOTA string cmds + peer IBRT sync.
 *
 * Phone (OP_TOTA_STRING, unencrypted):
 *   "cros get"
 *   "cros set poor=right mix=-20 bass=0 treble=0 vol=8 a2dp=12 noise=3"
 * Keys may be sent individually.
 *   poor=left|right|0|1
 *   mix=-30..-12 dB (HW sidetone; never 0 — howls)
 *   bass/treble=-6..+6 dB (SCO shelves on good ear)
 *   vol= / sco= 0..15 (HFP/SCO DAC gain on good ear — NOT music)
 *   a2dp= / music= 0..15 (A2DP music volume)
 *   noise=0..5 (soft gate + HF rolloff on good-ear SCO link hiss)
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
int cros_cfg_vol(void);
int cros_cfg_a2dp(void);
int cros_cfg_noise(void);

/*
 * Phone AVRCP Absolute Volume while BiCROS is on — dest_vol is
 * TGT_VOLUME_LEVEL_* (same as a2dp_volume_set). Maps to sco=0..15 and
 * applies HFP/SCO DAC on the good ear.
 */
void cros_cfg_on_abs_volume(int tgt_level);

/* Soft EQ + optional noise gate on SCO PCM (good ear). */
void cros_cfg_process_sco_pcm(uint8_t *buf, uint32_t len);

/* Re-apply sidetone gain / EQ coeffs after live change. */
void cros_cfg_apply_audio(void);

#ifdef __cplusplus
}
#endif

#endif /* CROS_CFG_H */
