/***************************************************************************
 * SCO/eSCO bud↔bud probe — OPEN/CLOSED (v0.3.37).
 *
 * No SCO work on enable (0.3.31 early register crashed RIGHT-master+TX).
 * Default (with extra): on peer READY → settle → register+open → auto-close
 * after OPENED (0.3.36 — SCO+extra wedged).
 *
 * CROS_SCO_ALONE=1 (0.3.37): skip extra; settle from enable; leave SCO up
 * until disable (prove SCO without extra L2CAP media).
 ***************************************************************************/
#include "cros_sco_probe.h"

#include "app_tws_ibrt.h"
#include "cmsis_os.h"
#include "cros_bt_log.h"
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

/* Safety net only — must be >> extra defer (2s) + PONG + settle. */
#define CROS_SCO_LATE_FALLBACK_MS 12000
#define CROS_SCO_SETTLE_MS 500
#define CROS_SCO_SETTLE_POOR_MS 1500
/* Alone mode: short settle from enable (no extra READY gate). */
#define CROS_SCO_ALONE_SETTLE_MS 1500
/* With extra: tear SCO down quickly after OPENED (0.3.35 hang). Alone: hold. */
#define CROS_SCO_PROOF_HOLD_MS 300
#define CROS_SCO_HCI_REMOTE_USER_TERM 0x13

static osTimerId late_timer;
static osTimerId settle_timer;
static osTimerId proof_timer;
static uint8_t sco_inited;
static uint8_t probe_armed;
static uint8_t registered;
static uint8_t open_issued;
static uint8_t sco_up;
static uint8_t voice_started;
static struct bdaddr_t peer_ba;
static uint8_t have_peer;

static void cros_sco_close_bt(void *a, void *b);

#if CROS_SCO_MEDIA
static osTimerId voice_timer;
#define CROS_SCO_VOICE_RETRY_MS 100

static uint16_t cros_sco_peer_handle(void) {
  ibrt_ctrl_t *ctx = app_tws_ibrt_get_bt_ctrl_ctx();
  uint16_t h = 0;
  if (ctx && ctx->tws_conhandle) {
    h = btif_me_get_scohdl_by_connhdl(ctx->tws_conhandle);
  }
  return h;
}

static void cros_sco_voice_start(void) {
  uint16_t sco_hdl;
  int rc;
  if (voice_started) {
    return;
  }
  sco_hdl = cros_sco_peer_handle();
  CROS_LOG(0, "[cros_sco] voice START try sco_hdl=0x%04x (CVSD via HFP path)",
           (unsigned)sco_hdl);
  if (!sco_hdl) {
    CROS_LOG(0, "[cros_sco] voice START — no handle yet, retry %ums",
             (unsigned)CROS_SCO_VOICE_RETRY_MS);
    if (voice_timer) {
      osTimerStop(voice_timer);
      osTimerStart(voice_timer, CROS_SCO_VOICE_RETRY_MS);
    }
    return;
  }
  /* Reuse phone-call voice player against peer SCO (first media probe). */
  rc = hfp_ibrt_sco_audio_connected(BTIF_HF_SCO_CODEC_CVSD, sco_hdl);
  voice_started = 1;
  CROS_LOG(0, "[cros_sco] voice START done rc=%d", rc);
}

static void cros_sco_voice_stop(void) {
  if (voice_timer) {
    osTimerStop(voice_timer);
  }
  if (!voice_started) {
    return;
  }
  CROS_LOG(0, "[cros_sco] voice STOP");
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
#if CROS_SCO_ALONE
#if CROS_SCO_MEDIA
    CROS_LOG(0, "[cros_sco] OPENED (alone + media — start voice path)");
    cros_sco_voice_start();
#else
    CROS_LOG(0, "[cros_sco] OPENED (alone hold — no extra, leave up until "
                "disable)");
#endif
#else
    CROS_LOG(0, "[cros_sco] OPENED (peer SCO up — proof ok, tearing down)");
    /* Do not leave peer SCO up under extra media / phone ACL (0.3.35 hang). */
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
#if CROS_SCO_MEDIA
    cros_sco_voice_stop();
#endif
    CROS_LOG(0, "[cros_sco] CLOSED");
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

  if (!probe_armed) {
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
    }
  }

  if (!do_open) {
    CROS_LOG(0, "[cros_sco] registered only — wait open");
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
    rc = sco_open_link(&peer_ba, cros_sco_notify, NULL);
    open_issued = 1;
    CROS_LOG(0, "[cros_sco] open_link rc=%d (await OPENED/CLOSED)", (int)rc);
  }
}

static void cros_sco_close_bt(void *a, void *b) {
  int8 rc;
  (void)a;
  (void)b;
  probe_armed = 0;
  open_issued = 0;
#if CROS_SCO_MEDIA
  cros_sco_voice_stop();
#endif
  if (have_peer) {
    if (sco_up || registered) {
      rc = sco_close_link(&peer_ba, CROS_SCO_HCI_REMOTE_USER_TERM);
      CROS_LOG(0, "[cros_sco] close_link rc=%d", (int)rc);
    }
    if (registered) {
      rc = sco_unregister_link(&peer_ba);
      CROS_LOG(0, "[cros_sco] unregister rc=%d", (int)rc);
      registered = 0;
    }
  }
  sco_up = 0;
  have_peer = 0;
}

static void late_timer_cb(void const *arg) {
  (void)arg;
  if (!probe_armed || open_issued) {
    return;
  }
  CROS_LOG(0, "[cros_sco] late fallback %ums — open (READY never came?)",
           (unsigned)CROS_SCO_LATE_FALLBACK_MS);
  app_bt_start_custom_function_in_bt_thread(1, 0, (uint32_t)cros_sco_open_bt);
}

static void settle_timer_cb(void const *arg) {
  (void)arg;
  if (!probe_armed || open_issued) {
    return;
  }
  CROS_LOG(0, "[cros_sco] settle done — register+open");
  app_bt_start_custom_function_in_bt_thread(1, 0, (uint32_t)cros_sco_open_bt);
}

static void proof_timer_cb(void const *arg) {
  (void)arg;
  CROS_LOG(0, "[cros_sco] proof hold done — close (free ACL for extra)");
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_sco_close_bt);
}

osTimerDef(CROS_SCO_LATE, late_timer_cb);
osTimerDef(CROS_SCO_SETTLE, settle_timer_cb);
osTimerDef(CROS_SCO_PROOF, proof_timer_cb);

void cros_sco_probe_init(void) {
  if (!late_timer) {
    late_timer = osTimerCreate(osTimer(CROS_SCO_LATE), osTimerOnce, NULL);
  }
  if (!settle_timer) {
    settle_timer = osTimerCreate(osTimer(CROS_SCO_SETTLE), osTimerOnce, NULL);
  }
  if (!proof_timer) {
    proof_timer = osTimerCreate(osTimer(CROS_SCO_PROOF), osTimerOnce, NULL);
  }
#if CROS_SCO_MEDIA
  if (!voice_timer) {
    voice_timer = osTimerCreate(osTimer(CROS_SCO_VOICE), osTimerOnce, NULL);
  }
#endif
#if CROS_SCO_ALONE
#if CROS_SCO_MEDIA
  CROS_LOG(1,
           "[cros_sco] probe init (ALONE+MEDIA, slave_open=%u — CVSD voice on "
           "OPENED)",
           (unsigned)CROS_SCO_SLAVE_OPEN);
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
  probe_armed = 1;
  open_issued = 0;
  if (!late_timer || !settle_timer || !proof_timer) {
    cros_sco_probe_init();
  }
  if (settle_timer) {
    osTimerStop(settle_timer);
  }
  if (proof_timer) {
    osTimerStop(proof_timer);
  }
  if (late_timer) {
    osTimerStop(late_timer);
  }
#if CROS_SCO_ALONE
  /* No extra READY — settle from enable, then open and hold. */
  CROS_LOG(0,
           "[cros_sco] armed ALONE — settle %ums then open (hold until "
           "disable)",
           (unsigned)CROS_SCO_ALONE_SETTLE_MS);
  osTimerStart(settle_timer, CROS_SCO_ALONE_SETTLE_MS);
#else
  /* v0.3.32: no sco_init/register on enable — that raced TX on RIGHT-master. */
  CROS_LOG(0,
           "[cros_sco] armed — WAIT peer READY (no early sco; late %ums)",
           (unsigned)CROS_SCO_LATE_FALLBACK_MS);
  osTimerStart(late_timer, CROS_SCO_LATE_FALLBACK_MS);
#endif
}

void cros_sco_probe_on_peer_ready(void) {
#if CROS_SCO_ALONE
  (void)0; /* Alone settles from enable; ignore extra READY if any. */
#else
  uint32_t settle_ms;
  if (!probe_armed) {
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
  if (late_timer) {
    osTimerStop(late_timer);
  }
  if (settle_timer) {
    osTimerStop(settle_timer);
  }
  if (proof_timer) {
    osTimerStop(proof_timer);
  }
  probe_armed = 0;
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_sco_close_bt);
}

#else /* !CROS_SCO_PROBE */

void cros_sco_probe_init(void) {}
void cros_sco_probe_on_cros_enable(void) {}
void cros_sco_probe_on_cros_disable(void) {}
void cros_sco_probe_on_peer_ready(void) {}

#endif /* CROS_SCO_PROBE */
