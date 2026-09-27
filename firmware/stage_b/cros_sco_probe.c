/***************************************************************************
 * SCO/eSCO bud↔bud probe — OPEN/CLOSED (v0.3.53).
 *
 * No SCO work on enable (0.3.31 early register crashed RIGHT-master+TX).
 * Default (with extra): on peer READY → settle → register+open → auto-close
 * after OPENED (0.3.36 — SCO+extra wedged).
 *
 * CROS_SCO_ALONE=1 (0.3.37): skip extra; settle from enable; leave SCO up
 * until disable (prove SCO without extra L2CAP media).
 *
 * v0.3.46: never register_link + open_link in the same BT-thread call —
 * open_link needs the HCI event loop between them (ear log 221736: silence
 * after register_link; taps/Apply dead until case). Gap timer splits them.
 *
 * v0.3.53: wait for real SCO CLOSED before unregister (voice drain →
 * close_link → soft 4s×3 → hard ~12s). Defer ENABLE while closing so every
 * re-open follows a clean CLOSED (ear 233307 second link).
 *
 * v0.3.58: sco_notify(CLOSED) rarely fires on peer SCO; HCI
 * BTEVENT_SCO_DISCONNECT is the real drop (ear ~20s later, err=0x22). After
 * soft close fails, await BTEVENT before unregister. Force teardown then
 * cools down before deferred ENABLE (ear fail: immediate rearm → OPENED
 * missing).
 ***************************************************************************/
#include "cros_sco_probe.h"

#include "app_tws_ibrt.h"
#include "cmsis_os.h"
#include "cros_bt_log.h"
#include "cros_cue.h"
#include "cros_tws.h"
#include "string.h"

#ifndef CROS_SCO_PROBE
#define CROS_SCO_PROBE 1
#endif

#ifndef CROS_SCO_SLAVE_OPEN
#define CROS_SCO_SLAVE_OPEN 0
#endif

#ifndef CROS_SCO_ALONE
#define CROS_SCO_ALONE 0
#endif

#ifndef CROS_SCO_MEDIA
#define CROS_SCO_MEDIA 0
#endif

#ifndef CROS_SCO_MSBC
#define CROS_SCO_MSBC 1
#endif

#if CROS_SCO_PROBE

#include "sco_i.h"
#if CROS_SCO_MEDIA
#include "app_ibrt_hf.h"
#include "hfp_api.h"
#include "me_api.h"
#endif

extern int app_bt_start_custom_function_in_bt_thread(uint32_t param0,
                                                     uint32_t param1,
                                                     uint32_t func);
extern bool app_tws_ibrt_tws_link_connected(void);
extern bt_bdaddr_t *btif_me_get_remote_device_bdaddr(btif_remote_device_t *rdev);
extern btif_remote_device_t *btif_besaud_get_peer_device(void);
extern btif_remote_device_t *
btif_me_get_remote_device_by_handle(uint16_t hci_handle);
#if CROS_SCO_MEDIA
extern int cros_sco_forcemute(int mic_mute, int spk_mute);
extern int cros_sco_set_hfp_volume(int level);
extern int cros_sco_get_hfp_volume(void);
extern void cros_sco_sidetone_set(int on);
extern void cros_sco_sidetone_set_gain_db(int db);
#include "cros_cfg.h"
#endif

/* Safety net only — must be >> extra defer (2s) + PONG + settle. */
#define CROS_SCO_LATE_FALLBACK_MS 12000
#define CROS_SCO_SETTLE_MS 500
#define CROS_SCO_SETTLE_POOR_MS 1500
/* Alone mode: settle from enable (no extra READY gate). */
#define CROS_SCO_ALONE_SETTLE_MS 1500
/* After BiCROS shape — settle before ENABLED cue (avoid AF race, v0.3.2). */
#define CROS_SCO_ENABLED_CUE_MS 500
/* After a clean CLOSED / BTEVENT teardown — settle then open. */
#define CROS_SCO_REARM_SETTLE_MS 3000
/* Gap so BT thread can process HCI between register_link and open_link. */
#define CROS_SCO_OPEN_GAP_MS 100
/* Let HFP voice path drop before close_link (helps CLOSED arrive). */
#define CROS_SCO_VOICE_DRAIN_MS 300
/* Hold voice_stop while DISABLED SCO-PCM cue finishes (ear 091029 silent). */
#define CROS_SCO_CUE_HOLD_MS 50
#define CROS_SCO_CUE_HOLD_MAX 12 /* 12×50ms = 600ms — covers 2×120+80 beeps */
/* Soft wait for SCO_CLOSED after close_link — do NOT unregister yet. */
#define CROS_SCO_CLOSE_WAIT_MS 4000
/* Soft re-close attempts before awaiting HCI disconnect (3×4s ≈ 12s). */
#define CROS_SCO_CLOSE_MAX_SOFT 3
/* Ear 074125: BTEVENT_SCO_DISCONNECT ~20s after close_link — keep waiting. */
#define CROS_SCO_HCI_WAIT_MS 20000
/* After forced unregister, cool before any deferred ENABLE (ear fail 074942). */
#define CROS_SCO_FORCE_COOLDOWN_MS 10000
/*
 * Absolute max time in closing / await-HCI / cool-down before we force-clear.
 * Soft≈12s + HCI≈20s + cool≈10s ≈ 42s; 75s leaves margin. Ear 093040: after
 * SPP drop, NOT_YET forever — timers can stall; this is the floor, not 100%.
 */
#define CROS_SCO_HOLD_ESCAPE_MS 75000
/* If open_link never yields OPENED, retry once. */
#define CROS_SCO_OPEN_RETRY_MS 2000
/* With extra: tear SCO down quickly after OPENED (0.3.35 hang). Alone: hold. */
#define CROS_SCO_PROOF_HOLD_MS 300
#define CROS_SCO_HCI_REMOTE_USER_TERM 0x13

static osTimerId late_timer;
static osTimerId settle_timer;
static osTimerId open_timer;
static osTimerId close_timer;
static osTimerId voice_drain_timer;
static osTimerId open_retry_timer;
static osTimerId proof_timer;
static osTimerId cooldown_timer;
static osTimerId enabled_cue_timer;
static osTimerId hold_escape_timer;
static uint8_t sco_inited;
static uint8_t probe_armed;
static uint8_t registered;
static uint8_t open_issued;
static uint8_t sco_up;
static uint8_t voice_started;
static uint8_t closing;
static uint8_t await_hci; /* soft close done — waiting BTEVENT_SCO_DISCONNECT */
static uint8_t close_attempts;
static uint8_t pending_enable; /* quad-tap on while still waiting CLOSED */
static uint8_t rearm; /* set after a completed session — longer settle */
static uint8_t open_retries;
static uint8_t cooldown_want_enable;
static uint8_t cue_hold_ticks; /* voice still up — waiting for SCO-PCM cue */
static uint8_t force_cooldown_pending; /* play READY after force cool-down */
static uint8_t enabled_cued; /* one ENABLED cue per OPENED session */
static struct bdaddr_t peer_ba;
static uint8_t have_peer;

static void cros_sco_close_bt(void *a, void *b);
static void cros_sco_schedule_open(void);
static void cros_sco_finish_teardown(const char *why, int force_cooldown);
static void cros_sco_finish_teardown_bt(void *a, void *b);
static void cros_sco_issue_close_link_bt(void *a, void *b);
static void cros_sco_voice_drain_bt(void *a, void *b);
static void cros_sco_hold_escape_bt(void *a, void *b);
static void cros_sco_arm_after_teardown(void);
static void cros_sco_arm_after_teardown_bt(void *a, void *b);
static void cros_sco_hold_escape_arm(void);
static void cros_sco_hold_escape_disarm(void);

int cros_sco_cfg_hold(void) {
  /* Skip IBRT cfg sync while peer SCO is up. */
  return sco_up ? 1 : 0;
}

int cros_sco_log_hold(void) {
  /* Pause SPP ring tee while CROS SCO is armed, opening, up, or closing. */
  return (probe_armed || sco_up || open_issued || closing || await_hci ||
          cooldown_want_enable || force_cooldown_pending)
             ? 1
             : 0;
}

int cros_sco_voice_is_up(void) {
  return (sco_up && voice_started) ? 1 : 0;
}

#if CROS_SCO_MEDIA
static osTimerId voice_timer;
static osTimerId cros_mute_timer;
#define CROS_SCO_VOICE_RETRY_MS 100
/* Player clears forcemute on open — apply CROS shape after it settles. */
#define CROS_SCO_CROS_MUTE_MS 400

static uint16_t cros_sco_peer_handle(void) {
  ibrt_ctrl_t *ctx = app_tws_ibrt_get_bt_ctrl_ctx();
  uint16_t h = 0;
  if (ctx && ctx->tws_conhandle) {
    h = btif_me_get_scohdl_by_connhdl(ctx->tws_conhandle);
  }
  return h;
}

/* Fallback only — live level comes from cros_cfg (default 8). */
#ifndef CROS_SCO_HFP_VOL
#define CROS_SCO_HFP_VOL 8
#endif

/* Asymmetric CROS / BiCROS on SCO:
 *  POOR: mic→SCO, spk off, no local sidetone
 *  GOOD: SCO→spk, mic not TX'd, HW sidetone mixes local (left) mic into spk
 */
static void cros_sco_apply_cros_mute(void) {
  int poor = cros_tws_is_poor_side() ? 1 : 0;
  int vol_before;
  int vol_after;
  int mix = (int)cros_cfg_mix_db();
  if (poor) {
    cros_sco_forcemute(0, 1);
    cros_sco_sidetone_set(0);
    CROS_LOG_ACK(0, "[cros_sco] CROS shape POOR/TX — mic ON, spk OFF, sidetone OFF");
  } else {
    /* Mute digital mic TX so local mic is not sent over SCO; HW sidetone still
     * taps ADC → DAC for local mix (BiCROS). Mix gain from cros_cfg. */
    cros_sco_forcemute(1, 0);
    cros_sco_sidetone_set_gain_db(mix);
    cros_sco_sidetone_set(1);
    vol_before = cros_sco_get_hfp_volume();
    vol_after = cros_sco_set_hfp_volume(cros_cfg_vol());
    CROS_LOG_ACK(0,
             "[cros_sco] BiCROS GOOD/RX — SCO+local mic mix, no TX; "
             "hfp_vol %d→%d sidetone ON mix=%ddB bass=%d treble=%d noise=%d "
             "a2dp=%d",
             vol_before, vol_after, mix, (int)cros_cfg_bass_db(),
             (int)cros_cfg_treble_db(), cros_cfg_noise(), cros_cfg_a2dp());
    /*
     * ENABLED cue after shape — SCO AF is already running. Short settle
     * avoids the v0.3.1 race (prompt vs stream bring-up). Once per session.
     */
    if (!enabled_cued && enabled_cue_timer) {
      enabled_cued = 1;
      osTimerStop(enabled_cue_timer);
      osTimerStart(enabled_cue_timer, CROS_SCO_ENABLED_CUE_MS);
    }
  }
}

void cros_sco_reapply_shape(void) {
  if (!sco_up || !voice_started) {
    return;
  }
  /* Role reshape only — avoid volume churn / sidetone tear-down on knob tweaks. */
  cros_sco_apply_cros_mute();
}

static void cros_mute_timer_cb(void const *arg) {
  (void)arg;
  if (!sco_up || !voice_started) {
    return;
  }
  cros_sco_apply_cros_mute();
}
osTimerDef(CROS_SCO_CROS_MUTE, cros_mute_timer_cb);

static void cros_sco_voice_start(void) {
  uint16_t sco_hdl;
  int rc;
  if (voice_started) {
    return;
  }
  sco_hdl = cros_sco_peer_handle();
#if CROS_SCO_MSBC
  CROS_LOG(0, "[cros_sco] voice START try sco_hdl=0x%04x (mSBC 16k + CROS)",
           (unsigned)sco_hdl);
#else
  CROS_LOG(0, "[cros_sco] voice START try sco_hdl=0x%04x (CVSD 8k + CROS)",
           (unsigned)sco_hdl);
#endif
  if (!sco_hdl) {
    CROS_LOG(0, "[cros_sco] voice START — no handle yet, retry %ums",
             (unsigned)CROS_SCO_VOICE_RETRY_MS);
    if (voice_timer) {
      osTimerStop(voice_timer);
      osTimerStart(voice_timer, CROS_SCO_VOICE_RETRY_MS);
    }
    return;
  }
#if CROS_SCO_MSBC
  {
    ibrt_ctrl_t *ctx = app_tws_ibrt_get_bt_ctrl_ctx();
    if (ctx) {
      ctx->ibrt_sco_codec = BTIF_HF_SCO_CODEC_MSBC;
    }
    rc = hfp_ibrt_sco_audio_connected(BTIF_HF_SCO_CODEC_MSBC, sco_hdl);
  }
#else
  rc = hfp_ibrt_sco_audio_connected(BTIF_HF_SCO_CODEC_CVSD, sco_hdl);
#endif
  voice_started = 1;
  CROS_LOG_ACK(0, "[cros_sco] voice UP rc=%d codec=%s",
           rc,
#if CROS_SCO_MSBC
           "mSBC/16k"
#else
           "CVSD/8k"
#endif
  );
  if (cros_mute_timer) {
    osTimerStop(cros_mute_timer);
    osTimerStart(cros_mute_timer, CROS_SCO_CROS_MUTE_MS);
  } else {
    cros_sco_apply_cros_mute();
  }
}

static void cros_sco_voice_stop(void) {
  if (voice_timer) {
    osTimerStop(voice_timer);
  }
  if (cros_mute_timer) {
    osTimerStop(cros_mute_timer);
  }
  if (!voice_started) {
    return;
  }
  CROS_LOG(0, "[cros_sco] voice STOP");
  cros_sco_sidetone_set(0);
  cros_sco_forcemute(0, 0);
  hfp_ibrt_sco_audio_disconnected();
  voice_started = 0;
}

static void voice_timer_cb(void const *arg) {
  (void)arg;
  if (!sco_up || voice_started) {
    return;
  }
  cros_sco_voice_start();
}
osTimerDef(CROS_SCO_VOICE, voice_timer_cb);
#endif /* CROS_SCO_MEDIA */

#if !CROS_SCO_MEDIA
void cros_sco_reapply_shape(void) {}
#endif

static void *cros_sco_peer_bdaddr(void) {
  btif_remote_device_t *dev;
  void *bd;
  ibrt_ctrl_t *ctx = app_tws_ibrt_get_bt_ctrl_ctx();

  dev = btif_besaud_get_peer_device();
  if (dev) {
    bd = btif_me_get_remote_device_bdaddr(dev);
    if (bd) {
      return bd;
    }
  }
  if (ctx && ctx->p_tws_remote_dev) {
    bd = btif_me_get_remote_device_bdaddr(ctx->p_tws_remote_dev);
    if (bd) {
      return bd;
    }
  }
  if (ctx && ctx->tws_conhandle) {
    dev = btif_me_get_remote_device_by_handle(ctx->tws_conhandle);
    if (dev) {
      bd = btif_me_get_remote_device_bdaddr(dev);
      if (bd) {
        return bd;
      }
    }
  }
  if (ctx) {
    return &ctx->peer_addr;
  }
  return NULL;
}

static void cros_sco_notify(enum sco_event_enum event, void *pdata,
                            void *link_host) {
  (void)pdata;
  (void)link_host;
  if (event == SCO_OPENED) {
    sco_up = 1;
    open_issued = 1; /* peer may have opened us — don't open_link again */
    open_retries = 0;
    if (open_timer) {
      osTimerStop(open_timer);
    }
    if (open_retry_timer) {
      osTimerStop(open_retry_timer);
    }
#if CROS_SCO_ALONE
#if CROS_SCO_MEDIA
    CROS_LOG_ACK(0, "[cros_sco] OPENED (alone + media — voice + BiCROS)");
    cros_sco_voice_start();
#else
    CROS_LOG_ACK(0, "[cros_sco] OPENED (alone hold — leave up until disable)");
#endif
#else
    CROS_LOG_ACK(0, "[cros_sco] OPENED (peer SCO up — proof ok, tearing down)");
    if (proof_timer) {
      osTimerStop(proof_timer);
      osTimerStart(proof_timer, CROS_SCO_PROOF_HOLD_MS);
    } else {
      app_bt_start_custom_function_in_bt_thread(0, 0,
                                                (uint32_t)cros_sco_close_bt);
    }
#endif
  } else if (event == SCO_CLOSED) {
    sco_up = 0;
    open_issued = 0;
#if CROS_SCO_MEDIA
    cros_sco_voice_stop();
#endif
    CROS_LOG_ACK(0, "[cros_sco] CLOSED");
    /* Unregister only after CLOSED — ear 233307: clean OPENED followed true CLOSED. */
    if (closing || await_hci) {
      cros_sco_finish_teardown("CLOSED", 0);
    }
  } else {
    CROS_LOG(0, "[cros_sco] notify event=%d", (int)event);
  }
}

static void cros_sco_open_bt(void *a, void *b) {
  ibrt_ctrl_t *ctx;
  void *bd;
  int8 rc;
  int should_open;
  int do_open = (a != NULL);
  (void)b;

  if (!probe_armed || closing || await_hci) {
    return;
  }
  if (!app_tws_ibrt_tws_link_connected()) {
    CROS_LOG(0, "[cros_sco] abort — TWS down before open");
    probe_armed = 0;
    return;
  }

  ctx = app_tws_ibrt_get_bt_ctrl_ctx();
  if (ctx && ctx->mobile_conhandle) {
    CROS_LOG(0,
             "[cros_sco] WARN mobile_conhandle=0x%04x — prefer phone "
             "disconnected for clean probe",
             (unsigned)ctx->mobile_conhandle);
  } else {
    CROS_LOG(0, "[cros_sco] mobile idle (good)");
  }

  bd = cros_sco_peer_bdaddr();
  if (!bd) {
    CROS_LOG(0, "[cros_sco] no TWS peer bdaddr — give up");
    probe_armed = 0;
    return;
  }
  memcpy(&peer_ba, bd, sizeof(peer_ba));
  have_peer = 1;

  CROS_LOG(0,
           "[cros_sco] peer %02x:%02x:%02x:%02x:%02x:%02x tws_mode=%u "
           "role=%u poor=%d slave_open=%u alone=%u do_open=%d",
           peer_ba.addr[0], peer_ba.addr[1], peer_ba.addr[2], peer_ba.addr[3],
           peer_ba.addr[4], peer_ba.addr[5],
           ctx ? (unsigned)ctx->tws_mode : 0xff,
           ctx ? (unsigned)ctx->current_role : 0xff,
           cros_tws_is_poor_side() ? 1 : 0, (unsigned)CROS_SCO_SLAVE_OPEN,
           (unsigned)CROS_SCO_ALONE, do_open);

  if (ctx && ctx->current_role == IBRT_MASTER && cros_tws_is_poor_side()) {
    CROS_LOG(0, "[cros_sco] WARN master+POOR/TX — 0.3.31 crash pattern; "
                "proceed after settle");
  }

  if (!sco_inited) {
    rc = sco_init();
    CROS_LOG(0, "[cros_sco] sco_init rc=%d", (int)rc);
    sco_inited = 1;
  }

  if (!registered) {
    rc = sco_register_link(&peer_ba, cros_sco_notify, NULL);
    CROS_LOG(0, "[cros_sco] register_link rc=%d", (int)rc);
    if (rc == 0 || rc == 2) {
      registered = 1;
    } else {
      CROS_LOG(0, "[cros_sco] register_link failed — abort open");
      probe_armed = 0;
      return;
    }
  }

  if (!do_open) {
    /* Return to BT event loop; open_link runs after CROS_SCO_OPEN_GAP_MS. */
    CROS_LOG(0, "[cros_sco] registered — open in %ums",
             (unsigned)CROS_SCO_OPEN_GAP_MS);
    cros_sco_schedule_open();
    return;
  }

  should_open = 0;
  if (ctx && ctx->current_role == IBRT_MASTER) {
    should_open = 1;
  } else if (ctx && ctx->current_role == IBRT_SLAVE) {
    if (CROS_SCO_SLAVE_OPEN) {
      should_open = 1;
      CROS_LOG(0, "[cros_sco] slave — also open_link (CROS_SCO_SLAVE_OPEN=1)");
    } else {
      CROS_LOG(0, "[cros_sco] slave — registered only, wait for peer open");
    }
  } else {
    should_open = 1;
    CROS_LOG(0, "[cros_sco] role unclear — this bud will open");
  }

  if (should_open && !open_issued) {
    /* Peer may already have brought SCO up (CONNECT_IND/OPENED). A second
     * open_link here causes bring-up dropouts (ear log 225910). */
    if (sco_up) {
      open_issued = 1;
      CROS_LOG_ACK(0, "[cros_sco] already OPENED — skip open_link");
      return;
    }
    rc = sco_open_link(&peer_ba, cros_sco_notify, NULL);
    open_issued = 1;
    CROS_LOG_ACK(0, "[cros_sco] open_link rc=%d (await OPENED/CLOSED)", (int)rc);
    if (open_retry_timer) {
      osTimerStop(open_retry_timer);
      osTimerStart(open_retry_timer, CROS_SCO_OPEN_RETRY_MS);
    }
  }
}

static void cros_sco_arm_after_teardown(void) {
  uint32_t settle_ms;

  probe_armed = 1;
  open_issued = 0;
  open_retries = 0;
  if (!settle_timer || !open_timer) {
    cros_sco_probe_init();
  }
  if (settle_timer) {
    osTimerStop(settle_timer);
  }
  if (open_timer) {
    osTimerStop(open_timer);
  }
  if (open_retry_timer) {
    osTimerStop(open_retry_timer);
  }
#if CROS_SCO_ALONE
  settle_ms = rearm ? CROS_SCO_REARM_SETTLE_MS : CROS_SCO_ALONE_SETTLE_MS;
  CROS_LOG_ACK(0, "[cros_tws] ENABLE (settle %ums%s)", (unsigned)settle_ms,
               rearm ? ", rearm" : "");
  osTimerStart(settle_timer, settle_ms);
#else
  CROS_LOG_ACK(0, "[cros_tws] ENABLE (wait READY)");
  if (late_timer) {
    osTimerStop(late_timer);
    osTimerStart(late_timer, CROS_SCO_LATE_FALLBACK_MS);
  }
#endif
}

static void cros_sco_arm_after_teardown_bt(void *a, void *b) {
  (void)a;
  (void)b;
  cros_sco_arm_after_teardown();
}

static void cros_sco_finish_teardown(const char *why, int force_cooldown) {
  int8 rc;
  uint8_t want_enable;

  if (close_timer) {
    osTimerStop(close_timer);
  }
  if (voice_drain_timer) {
    osTimerStop(voice_drain_timer);
  }
  if (open_retry_timer) {
    osTimerStop(open_retry_timer);
  }
  if (enabled_cue_timer) {
    osTimerStop(enabled_cue_timer);
  }
#if CROS_SCO_MEDIA
  cros_sco_voice_stop();
#endif
  if (have_peer && registered) {
    rc = sco_unregister_link(&peer_ba);
    CROS_LOG(0, "[cros_sco] unregister rc=%d (%s)", (int)rc, why ? why : "?");
    registered = 0;
  }
  sco_up = 0;
  open_issued = 0;
  probe_armed = 0;
  have_peer = 0;
  closing = 0;
  await_hci = 0;
  close_attempts = 0;
  open_retries = 0;
  sco_inited = 0;
  enabled_cued = 0;
  rearm = 1;
  want_enable = pending_enable;
  pending_enable = 0;
  CROS_LOG_ACK(0, "[cros_tws] DISABLE (%s)", why ? why : "done");
  if (force_cooldown) {
    /*
     * Controller often still holds SCO after forced unregister (ear:
     * BTEVENT arrives ~20s later). Immediate deferred ENABLE → open_link
     * rc=0 but never OPENED. Always cool, then READY cue (safe to re-arm).
     */
    force_cooldown_pending = 1;
    cooldown_want_enable = want_enable ? 1 : 0;
    cros_sco_hold_escape_arm(); /* cover cool-down stall too */
    if (!cooldown_timer) {
      cros_sco_probe_init();
    }
    if (cooldown_timer) {
      osTimerStop(cooldown_timer);
      osTimerStart(cooldown_timer, CROS_SCO_FORCE_COOLDOWN_MS);
      CROS_LOG_ACK(0,
                   "[cros_sco] force teardown — cool %ums then READY%s",
                   (unsigned)CROS_SCO_FORCE_COOLDOWN_MS,
                   want_enable ? "+ENABLE" : "");
    } else {
      cros_sco_hold_escape_disarm();
      cros_cue_ready();
      if (want_enable) {
        cros_sco_arm_after_teardown();
      }
    }
    return;
  }
  /* Clean CLOSED / BTEVENT — controller free now. */
  cros_sco_hold_escape_disarm();
  cros_cue_ready();
  if (want_enable) {
    CROS_LOG_ACK(0, "[cros_sco] CLOSED done — run deferred ENABLE");
    cros_sco_arm_after_teardown();
  }
}

static void cros_sco_finish_teardown_bt(void *a, void *b) {
  const char *why = "hard-timeout";
  int force = 1;
  (void)b;
  if (a) {
    why = (const char *)a;
    /* Non-forced paths pass why via other callers; this BT entry is force. */
    force = 1;
  }
  cros_sco_finish_teardown(why, force);
}

static void cros_sco_issue_close_link_bt(void *a, void *b) {
  int8 rc;
  (void)a;
  (void)b;
  if (!closing) {
    return;
  }
  if (!have_peer) {
    cros_sco_finish_teardown("idle", 0);
    return;
  }
  rc = sco_close_link(&peer_ba, CROS_SCO_HCI_REMOTE_USER_TERM);
  CROS_LOG_ACK(0, "[cros_sco] close_link rc=%d — wait CLOSED (try %u)", (int)rc,
               (unsigned)(close_attempts + 1));
  if (close_timer) {
    osTimerStop(close_timer);
    osTimerStart(close_timer, CROS_SCO_CLOSE_WAIT_MS);
  } else {
    cros_sco_finish_teardown("no-close-timer", 1);
  }
}

static void cros_sco_hold_escape_arm(void) {
  if (!hold_escape_timer) {
    return;
  }
  osTimerStop(hold_escape_timer);
  osTimerStart(hold_escape_timer, CROS_SCO_HOLD_ESCAPE_MS);
}

static void cros_sco_hold_escape_disarm(void) {
  if (hold_escape_timer) {
    osTimerStop(hold_escape_timer);
  }
}

static void cros_sco_hold_escape_bt(void *a, void *b) {
  uint8_t want;
  (void)a;
  (void)b;
  if (!(closing || await_hci || force_cooldown_pending || cooldown_want_enable)) {
    return;
  }
  want = (uint8_t)(pending_enable || cooldown_want_enable);
  CROS_LOG_ACK(0, "[cros_sco] HOLD ESCAPE — clear deferred-enable wedge (want=%u)",
               (unsigned)want);
  if (cooldown_timer) {
    osTimerStop(cooldown_timer);
  }
  force_cooldown_pending = 0;
  cooldown_want_enable = 0;
  pending_enable = want;
  if (closing || await_hci || registered || sco_up
#if CROS_SCO_MEDIA
      || voice_started
#endif
  ) {
    /* Force path plays READY after cool; preserves deferred ENABLE. */
    cros_sco_finish_teardown("hold-escape", 1);
    return;
  }
  cros_sco_hold_escape_disarm();
  cros_cue_ready();
  if (want) {
    pending_enable = 0;
    cros_sco_arm_after_teardown();
  }
}

static void hold_escape_timer_cb(void const *arg) {
  (void)arg;
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_sco_hold_escape_bt);
}

static void cros_sco_close_bt(void *a, void *b) {
  (void)a;
  (void)b;
  probe_armed = 0;
  open_issued = 0;
  if (open_timer) {
    osTimerStop(open_timer);
  }
  if (open_retry_timer) {
    osTimerStop(open_retry_timer);
  }
  if (closing) {
    return;
  }
  closing = 1;
  await_hci = 0;
  close_attempts = 0;
  cue_hold_ticks = 0;
  cros_sco_hold_escape_arm();
  if (have_peer && (sco_up || registered)) {
    /*
     * Drain voice first, then close_link, then wait for real CLOSED / HCI
     * disconnect before unregister (ear 233307 / 074125).
     * If DISABLED SCO-PCM cue is still playing, keep voice up until it
     * finishes — otherwise the mix has no PCM and the cue is silent (091029).
     */
#if CROS_SCO_MEDIA
    if (voice_started && cros_cue_sco_busy() && voice_drain_timer) {
      osTimerStop(voice_drain_timer);
      osTimerStart(voice_drain_timer, CROS_SCO_CUE_HOLD_MS);
      CROS_LOG_ACK(0, "[cros_sco] cue hold — voice stays up for DISABLED");
      return;
    }
    cros_sco_voice_stop();
#endif
    if (voice_drain_timer) {
      osTimerStop(voice_drain_timer);
      osTimerStart(voice_drain_timer, CROS_SCO_VOICE_DRAIN_MS);
      CROS_LOG_ACK(0, "[cros_sco] voice drained — close in %ums",
                   (unsigned)CROS_SCO_VOICE_DRAIN_MS);
    } else {
      cros_sco_issue_close_link_bt(NULL, NULL);
    }
    return;
  }
#if CROS_SCO_MEDIA
  cros_sco_voice_stop();
#endif
  cros_sco_finish_teardown("idle", 0);
}

static void cros_sco_voice_drain_bt(void *a, void *b) {
  (void)a;
  (void)b;
  if (!closing) {
    return;
  }
#if CROS_SCO_MEDIA
  /* Keep voice up until DISABLED SCO-PCM cue finishes, then stop + drain. */
  if (voice_started) {
    if (cros_cue_sco_busy() && cue_hold_ticks < CROS_SCO_CUE_HOLD_MAX) {
      cue_hold_ticks++;
      if (voice_drain_timer) {
        osTimerStart(voice_drain_timer, CROS_SCO_CUE_HOLD_MS);
      }
      return;
    }
    cros_sco_voice_stop();
    cue_hold_ticks = 0;
    if (voice_drain_timer) {
      osTimerStop(voice_drain_timer);
      osTimerStart(voice_drain_timer, CROS_SCO_VOICE_DRAIN_MS);
      CROS_LOG_ACK(0, "[cros_sco] voice drained — close in %ums",
                   (unsigned)CROS_SCO_VOICE_DRAIN_MS);
    } else {
      cros_sco_issue_close_link_bt(NULL, NULL);
    }
    return;
  }
#endif
  cros_sco_issue_close_link_bt(NULL, NULL);
}

static void voice_drain_timer_cb(void const *arg) {
  (void)arg;
  if (!closing) {
    return;
  }
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_sco_voice_drain_bt);
}

static void cros_sco_schedule_open(void) {
  if (!open_timer) {
    return;
  }
  osTimerStop(open_timer);
  osTimerStart(open_timer, CROS_SCO_OPEN_GAP_MS);
}

static void open_timer_cb(void const *arg) {
  (void)arg;
  if (!probe_armed || open_issued || closing || await_hci) {
    return;
  }
  if (sco_up) {
    open_issued = 1;
    CROS_LOG_ACK(0, "[cros_sco] open gap — already OPENED, skip open_link");
    return;
  }
  CROS_LOG(0, "[cros_sco] open gap done — open_link");
  app_bt_start_custom_function_in_bt_thread(1, 0, (uint32_t)cros_sco_open_bt);
}

static void open_retry_bt(void *a, void *b) {
  (void)a;
  (void)b;
  if (!probe_armed || sco_up || closing || await_hci) {
    return;
  }
  if (open_retries >= 1) {
    CROS_LOG_ACK(0, "[cros_sco] OPENED missing after retry — give up");
    probe_armed = 0;
    open_issued = 0;
    cros_cue_open_fail();
    return;
  }
  open_retries++;
  open_issued = 0;
  if (have_peer && registered) {
    (void)sco_unregister_link(&peer_ba);
    registered = 0;
  }
  sco_inited = 0;
  CROS_LOG_ACK(0, "[cros_sco] OPENED missing — retry register+open");
  cros_sco_open_bt(NULL, NULL);
}

static void open_retry_timer_cb(void const *arg) {
  (void)arg;
  if (!probe_armed || sco_up || closing || await_hci) {
    return;
  }
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)open_retry_bt);
}

static void close_timer_cb(void const *arg) {
  (void)arg;
  if (!closing) {
    return;
  }
  if (await_hci) {
    /* Soft + HCI wait exhausted — force unregister; cool before rearm. */
    CROS_LOG_ACK(0, "[cros_sco] CLOSED hard-timeout — force teardown");
    app_bt_start_custom_function_in_bt_thread((uint32_t)"hard-timeout", 0,
                                              (uint32_t)cros_sco_finish_teardown_bt);
    return;
  }
  close_attempts++;
  if (close_attempts < CROS_SCO_CLOSE_MAX_SOFT) {
    CROS_LOG_ACK(0, "[cros_sco] CLOSED pending — re-close (try %u/%u)",
                 (unsigned)(close_attempts + 1),
                 (unsigned)CROS_SCO_CLOSE_MAX_SOFT);
    app_bt_start_custom_function_in_bt_thread(
        0, 0, (uint32_t)cros_sco_issue_close_link_bt);
    return;
  }
  /*
   * sco_notify(CLOSED) rarely arrives on peer SCO. Keep registered and wait
   * for BTEVENT_SCO_DISCONNECT (ear ~20s) before unregistering.
   */
  await_hci = 1;
  CROS_LOG_ACK(0,
               "[cros_sco] CLOSED soft done — await BTEVENT up to %ums "
               "(do not unregister yet)",
               (unsigned)CROS_SCO_HCI_WAIT_MS);
  if (close_timer) {
    osTimerStop(close_timer);
    osTimerStart(close_timer, CROS_SCO_HCI_WAIT_MS);
  } else {
    app_bt_start_custom_function_in_bt_thread((uint32_t)"hard-timeout", 0,
                                              (uint32_t)cros_sco_finish_teardown_bt);
  }
}

static void cooldown_timer_cb(void const *arg) {
  (void)arg;
  if (!force_cooldown_pending && !cooldown_want_enable && !pending_enable) {
    return;
  }
  force_cooldown_pending = 0;
  cooldown_want_enable = cooldown_want_enable || pending_enable;
  pending_enable = 0;
  cros_sco_hold_escape_disarm();
  cros_cue_ready();
  if (!cooldown_want_enable) {
    return;
  }
  cooldown_want_enable = 0;
  CROS_LOG_ACK(0, "[cros_sco] cool-down done — deferred ENABLE");
  app_bt_start_custom_function_in_bt_thread(
      0, 0, (uint32_t)cros_sco_arm_after_teardown_bt);
}

static void enabled_cue_timer_cb(void const *arg) {
  (void)arg;
  if (!sco_up || !voice_started) {
    return;
  }
  cros_cue_enabled();
}

/*
 * HCI SCO disconnect (app_bt global BTEVENT tee). Peer SCO often never
 * delivers sco_notify(CLOSED); this is the reliable teardown signal.
 * Ignore non-peer remotes (phone HFP SCO).
 */
void cros_sco_on_hci_disconnect(uint8_t err, const uint8_t *rem6) {
  int peer_match = 0;

  if (!closing && !await_hci && !sco_up) {
    return;
  }
  if (have_peer && rem6) {
    peer_match = (rem6[0] == peer_ba.addr[0] && rem6[1] == peer_ba.addr[1] &&
                  rem6[2] == peer_ba.addr[2] && rem6[3] == peer_ba.addr[3] &&
                  rem6[4] == peer_ba.addr[4] && rem6[5] == peer_ba.addr[5]);
    if (!peer_match) {
      CROS_LOG(0, "[cros_sco] BTEVENT disconnect ignored (not peer) err=0x%02x",
               (unsigned)err);
      return;
    }
  }
  if (!(closing || await_hci || sco_up)) {
    return;
  }
  CROS_LOG_ACK(0, "[cros_sco] BTEVENT disconnect → teardown err=0x%02x",
               (unsigned)err);
  sco_up = 0;
  open_issued = 0;
#if CROS_SCO_MEDIA
  cros_sco_voice_stop();
#endif
  if (closing || await_hci) {
    cros_sco_finish_teardown("BTEVENT", 0);
  } else {
    /* Peer dropped while we thought SCO was up — clean without force cool-down. */
    closing = 1;
    cros_sco_finish_teardown("BTEVENT-drop", 0);
  }
}

int cros_sco_rem_is_peer(const uint8_t *rem6) {
  if (!rem6 || !have_peer) {
    return 0;
  }
  return (rem6[0] == peer_ba.addr[0] && rem6[1] == peer_ba.addr[1] &&
          rem6[2] == peer_ba.addr[2] && rem6[3] == peer_ba.addr[3] &&
          rem6[4] == peer_ba.addr[4] && rem6[5] == peer_ba.addr[5])
             ? 1
             : 0;
}

int cros_sco_peer_bdaddr_copy(uint8_t out[6]) {
  if (!out || !have_peer) {
    return -1;
  }
  out[0] = peer_ba.addr[0];
  out[1] = peer_ba.addr[1];
  out[2] = peer_ba.addr[2];
  out[3] = peer_ba.addr[3];
  out[4] = peer_ba.addr[4];
  out[5] = peer_ba.addr[5];
  return 0;
}

static void late_timer_cb(void const *arg) {
  (void)arg;
  if (!probe_armed || open_issued || closing) {
    return;
  }
  CROS_LOG(0, "[cros_sco] late fallback %ums — register then open",
           (unsigned)CROS_SCO_LATE_FALLBACK_MS);
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_sco_open_bt);
}

static void settle_timer_cb(void const *arg) {
  (void)arg;
  if (closing || await_hci) {
    /* Still tearing down — check again shortly. */
    if (settle_timer) {
      osTimerStart(settle_timer, 500);
    }
    return;
  }
  if (!probe_armed || open_issued) {
    return;
  }
  CROS_LOG(0, "[cros_sco] settle done — register (open after %ums gap)",
           (unsigned)CROS_SCO_OPEN_GAP_MS);
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_sco_open_bt);
}

static void proof_timer_cb(void const *arg) {
  (void)arg;
  CROS_LOG(0, "[cros_sco] proof hold done — close (free ACL for extra)");
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_sco_close_bt);
}

osTimerDef(CROS_SCO_LATE, late_timer_cb);
osTimerDef(CROS_SCO_SETTLE, settle_timer_cb);
osTimerDef(CROS_SCO_OPEN, open_timer_cb);
osTimerDef(CROS_SCO_CLOSE, close_timer_cb);
osTimerDef(CROS_SCO_VOICE_DRAIN, voice_drain_timer_cb);
osTimerDef(CROS_SCO_OPEN_RETRY, open_retry_timer_cb);
osTimerDef(CROS_SCO_PROOF, proof_timer_cb);
osTimerDef(CROS_SCO_COOLDOWN, cooldown_timer_cb);
osTimerDef(CROS_SCO_ENABLED_CUE, enabled_cue_timer_cb);
osTimerDef(CROS_SCO_HOLD_ESCAPE, hold_escape_timer_cb);

void cros_sco_probe_init(void) {
  if (!late_timer) {
    late_timer = osTimerCreate(osTimer(CROS_SCO_LATE), osTimerOnce, NULL);
  }
  if (!settle_timer) {
    settle_timer = osTimerCreate(osTimer(CROS_SCO_SETTLE), osTimerOnce, NULL);
  }
  if (!open_timer) {
    open_timer = osTimerCreate(osTimer(CROS_SCO_OPEN), osTimerOnce, NULL);
  }
  if (!close_timer) {
    close_timer = osTimerCreate(osTimer(CROS_SCO_CLOSE), osTimerOnce, NULL);
  }
  if (!voice_drain_timer) {
    voice_drain_timer =
        osTimerCreate(osTimer(CROS_SCO_VOICE_DRAIN), osTimerOnce, NULL);
  }
  if (!open_retry_timer) {
    open_retry_timer =
        osTimerCreate(osTimer(CROS_SCO_OPEN_RETRY), osTimerOnce, NULL);
  }
  if (!proof_timer) {
    proof_timer = osTimerCreate(osTimer(CROS_SCO_PROOF), osTimerOnce, NULL);
  }
  if (!cooldown_timer) {
    cooldown_timer =
        osTimerCreate(osTimer(CROS_SCO_COOLDOWN), osTimerOnce, NULL);
  }
  if (!enabled_cue_timer) {
    enabled_cue_timer =
        osTimerCreate(osTimer(CROS_SCO_ENABLED_CUE), osTimerOnce, NULL);
  }
  if (!hold_escape_timer) {
    hold_escape_timer =
        osTimerCreate(osTimer(CROS_SCO_HOLD_ESCAPE), osTimerOnce, NULL);
  }
#if CROS_SCO_MEDIA
  if (!voice_timer) {
    voice_timer = osTimerCreate(osTimer(CROS_SCO_VOICE), osTimerOnce, NULL);
  }
  if (!cros_mute_timer) {
    cros_mute_timer =
        osTimerCreate(osTimer(CROS_SCO_CROS_MUTE), osTimerOnce, NULL);
  }
#endif
#if CROS_SCO_ALONE
#if CROS_SCO_MEDIA
  CROS_LOG(1,
           "[cros_sco] probe init (ALONE+MEDIA+CROS, slave_open=%u — %s poor "
           "TX / good RX + BiCROS sidetone, hfp_vol bump)",
           (unsigned)CROS_SCO_SLAVE_OPEN,
#if CROS_SCO_MSBC
           "mSBC/16k"
#else
           "CVSD/8k"
#endif
  );
#else
  CROS_LOG(1,
           "[cros_sco] probe init (ALONE hold, slave_open=%u — no extra, leave "
           "OPENED up)",
           (unsigned)CROS_SCO_SLAVE_OPEN);
#endif
#else
  CROS_LOG(1,
           "[cros_sco] probe init (READY+settle, slave_open=%u, auto-close "
           "after OPENED)",
           (unsigned)CROS_SCO_SLAVE_OPEN);
#endif
}

void cros_sco_probe_on_cros_enable(void) {
  /* Still waiting for CLOSED / HCI drop / force cool-down — do not open yet. */
  if (closing || await_hci || force_cooldown_pending || cooldown_want_enable) {
    pending_enable = 1;
    rearm = 1;
    cros_cue_not_yet();
    CROS_LOG_ACK(0,
                 "[cros_tws] ENABLE deferred — waiting %s",
                 closing ? (await_hci ? "BTEVENT" : "CLOSED")
                         : "cool-down");
    return;
  }
  pending_enable = 0;
  cros_sco_arm_after_teardown();
}

void cros_sco_probe_on_peer_ready(void) {
#if CROS_SCO_ALONE
  (void)0; /* Alone settles from enable; ignore extra READY if any. */
#else
  uint32_t settle_ms;
  if (!probe_armed || closing || await_hci) {
    return;
  }
  if (open_issued) {
    return;
  }
  if (late_timer) {
    osTimerStop(late_timer);
  }
  settle_ms = cros_tws_is_poor_side() ? CROS_SCO_SETTLE_POOR_MS
                                      : CROS_SCO_SETTLE_MS;
  CROS_LOG(0, "[cros_sco] peer READY — settle %ums then open (poor=%d)",
           (unsigned)settle_ms, cros_tws_is_poor_side() ? 1 : 0);
  if (!settle_timer) {
    cros_sco_probe_init();
  }
  osTimerStop(settle_timer);
  osTimerStart(settle_timer, settle_ms);
#endif
}

void cros_sco_probe_on_cros_disable(void) {
  pending_enable = 0;
  cooldown_want_enable = 0;
  /* DISABLED cue at request time — teardown may still take tens of seconds. */
  cros_cue_disabled();
  if (late_timer) {
    osTimerStop(late_timer);
  }
  if (settle_timer) {
    osTimerStop(settle_timer);
  }
  if (open_timer) {
    osTimerStop(open_timer);
  }
  if (open_retry_timer) {
    osTimerStop(open_retry_timer);
  }
  if (proof_timer) {
    osTimerStop(proof_timer);
  }
  if (cooldown_timer) {
    osTimerStop(cooldown_timer);
  }
  force_cooldown_pending = 0;
  if (enabled_cue_timer) {
    osTimerStop(enabled_cue_timer);
  }
  /* Do NOT stop close_timer / voice_drain — mid-teardown must finish. */
  probe_armed = 0;
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_sco_close_bt);
}

#else /* !CROS_SCO_PROBE */

void cros_sco_probe_init(void) {}
void cros_sco_probe_on_cros_enable(void) {}
void cros_sco_probe_on_cros_disable(void) {}
void cros_sco_probe_on_peer_ready(void) {}
void cros_sco_reapply_shape(void) {}
void cros_sco_on_hci_disconnect(uint8_t err, const uint8_t *rem6) {
  (void)err;
  (void)rem6;
}
int cros_sco_rem_is_peer(const uint8_t *rem6) {
  (void)rem6;
  return 0;
}
int cros_sco_peer_bdaddr_copy(uint8_t out[6]) {
  (void)out;
  return -1;
}
int cros_sco_cfg_hold(void) { return 0; }
int cros_sco_log_hold(void) { return 0; }
int cros_sco_voice_is_up(void) { return 0; }

#endif /* CROS_SCO_PROBE */
