/***************************************************************************
 * BESAUD extra L2CAP transport for CROS audio packets.
 ***************************************************************************/
#include "cros_besaud_extra.h"

#include "app_tws_besaud.h"
#include "besaud_api.h"
#include "co_ppbuff.h"
#include "hal_trace.h"
#include "l2cap_i.h"
#include "me_api.h"
#include "string.h"

/* Avoid app_bt_func.h (pulls a2dp_api → codec_sbc). */
extern int app_bt_start_custom_function_in_bt_thread(uint32_t param0,
                                                     uint32_t param1,
                                                     uint32_t funcPtr);
extern bool app_tws_ibrt_tws_link_connected(void);

/* Forward: delivered from cros_tws.c */
extern void cros_tws_on_peer_audio(uint8_t *data, uint16_t len);

#define CROS_EXTRA_TX_MAX 512

static volatile uint32_t extra_handle;
static volatile uint8_t extra_open;
static volatile uint8_t create_issued;
static volatile uint8_t tx_busy;
static uint8_t tx_scratch[CROS_EXTRA_TX_MAX];
static uint16_t tx_scratch_len;
static uint32_t tx_ok;
static uint32_t tx_fail;
static uint32_t rx_ok;

static int cros_extra_notify(enum l2cap_event_enum event, uint32 l2cap_handle,
                             void *pdata, uint8 reason) {
  (void)pdata;
  (void)reason;
  switch (event) {
  case L2CAP_CHANNEL_OPENED:
  case L2CAP_CHANNEL_NEW_OPENED:
    extra_handle = l2cap_handle;
    extra_open = 1;
    tx_busy = 0;
    TRACE(2, "[cros_extra] OPEN handle=0x%08x", (unsigned)l2cap_handle);
    break;
  case L2CAP_CHANNEL_TX_HANDLED:
    tx_busy = 0;
    break;
  case L2CAP_CHANNEL_CLOSED:
    TRACE(1, "[cros_extra] CLOSED handle=0x%08x", (unsigned)l2cap_handle);
    if (extra_handle == l2cap_handle) {
      extra_handle = 0;
      extra_open = 0;
      tx_busy = 0;
      create_issued = 0;
    }
    break;
  default:
    break;
  }
  return 0;
}

static void cros_extra_datarecv(uint32 l2cap_handle, struct pp_buff *ppb) {
  (void)l2cap_handle;
  if (!ppb || !ppb->data || ppb->len == 0) {
    return;
  }
  /* Stack owns ppb — copy out, do not ppb_free (matches stock stub). */
  cros_tws_on_peer_audio(ppb->data, (uint16_t)ppb->len);
  rx_ok++;
  if ((rx_ok & 0x3F) == 0) {
    TRACE(2, "[cros_extra] rx=%u", (unsigned)rx_ok);
  }
}

static void *cros_extra_peer_addr(void) {
  btif_remote_device_t *dev = btif_besaud_get_peer_device();
  if (dev) {
    return btif_me_get_remote_device_bdaddr(dev);
  }
  return NULL;
}

static void cros_extra_create_bt(void *a, void *b) {
  void *remote;
  (void)a;
  (void)b;
  if (extra_open) {
    return;
  }
  remote = cros_extra_peer_addr();
  if (!remote) {
    TRACE(0, "[cros_extra] create skipped — no peer addr");
    create_issued = 0;
    return;
  }
  if (!tws_besaud_is_connected() && !app_tws_ibrt_tws_link_connected()) {
    TRACE(0, "[cros_extra] create skipped — BESAUD/TWS down");
    create_issued = 0;
    return;
  }
  TRACE(0, "[cros_extra] l2cap_create_besaud_extra_channel 0x0b0e");
  l2cap_create_besaud_extra_channel(remote, L2CAP_BESAUD_EXTRA_CHAN_ID,
                                    cros_extra_notify, cros_extra_datarecv);
}

static void cros_extra_send_bt(void *a, void *b) {
  int8_t ret;
  (void)a;
  (void)b;
  if (!extra_open || !extra_handle || tx_scratch_len == 0) {
    tx_busy = 0;
    return;
  }
  ret = l2cap_send_data(extra_handle, tx_scratch, tx_scratch_len, NULL);
  if (ret != 0) {
    tx_busy = 0;
    tx_fail++;
    TRACE(2, "[cros_extra] send fail %d (fail=%u)", (int)ret, (unsigned)tx_fail);
  } else {
    tx_ok++;
    if ((tx_ok & 0x3F) == 0) {
      TRACE(2, "[cros_extra] tx=%u fail=%u", (unsigned)tx_ok, (unsigned)tx_fail);
    }
  }
}

void cros_besaud_extra_init(void) {
  extra_handle = 0;
  extra_open = 0;
  create_issued = 0;
  tx_busy = 0;
  tx_scratch_len = 0;
  tx_ok = tx_fail = rx_ok = 0;
}

void cros_besaud_extra_ensure(void) {
  if (extra_open || create_issued) {
    return;
  }
  create_issued = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0,
                                            (uint32_t)cros_extra_create_bt);
}

void cros_besaud_extra_on_besaud_down(void) {
  extra_handle = 0;
  extra_open = 0;
  create_issued = 0;
  tx_busy = 0;
}

bool cros_besaud_extra_is_open(void) { return extra_open != 0; }

bool cros_besaud_extra_tx_busy(void) { return tx_busy != 0; }

void cros_besaud_extra_force_clear_pending(void) { tx_busy = 0; }

int cros_besaud_extra_send(const uint8_t *data, uint16_t len) {
  if (!extra_open || !data || len == 0 || len > CROS_EXTRA_TX_MAX) {
    return -1;
  }
  if (tx_busy) {
    return -2;
  }
  memcpy(tx_scratch, data, len);
  tx_scratch_len = len;
  tx_busy = 1;
  app_bt_start_custom_function_in_bt_thread(0, 0, (uint32_t)cros_extra_send_bt);
  return 0;
}
