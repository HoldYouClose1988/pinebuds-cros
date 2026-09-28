/***************************************************************************
 * TWS ACL poll / duration read-back (Task B from closed-lib briefing).
 *
 * Behavior notes only — no disassembly in-tree. Uses open-source reg_op
 * getters (patch 0015) + ibrt_ctrl config mirrors of the vendor UI macros.
 ***************************************************************************/
#include "cros_link_diag.h"

#include "app_tws_ibrt.h"
#include "bt_drv_reg_op.h"
#include "cros_bt_log.h"

void cros_link_diag_log(const char *where) {
  ibrt_ctrl_t *p;
  uint16_t poll = 0xffff;
  uint16_t poll_sco = 0xffff;
  uint8_t duration = 0xff;
  uint16_t tpoll = 0xffff;
  uint16_t tws_hdl = 0xffff;
  uint8_t linkid = 0xff;
  const char *tag = where ? where : "?";

  p = app_tws_ibrt_get_bt_ctrl_ctx();
  if (p) {
    tws_hdl = p->tws_conhandle;
    if (tws_hdl != 0xffff && tws_hdl >= 0x80) {
      linkid = (uint8_t)(tws_hdl - 0x80);
    }
  }

  btdrv_reg_op_get_private_tws_poll_interval(&poll, &poll_sco);
  duration = btdrv_reg_op_get_tws_link_duration();
  if (linkid < 3) {
    tpoll = bt_drv_reg_op_get_tpoll(linkid);
  }

  CROS_LOG_ACK(
      0,
      "[cros_link] %s cfg def=0x%x long=0x%x short=0x%x sco=0x%x short_sco=0x%x "
      "| live poll=0x%x/0x%x dur=%u tpoll=0x%x hdl=0x%x lid=%u",
      tag,
      p ? (unsigned)p->config.default_private_poll_interval : 0u,
      p ? (unsigned)p->config.long_private_poll_interval : 0u,
      p ? (unsigned)p->config.short_private_poll_interval : 0u,
      p ? (unsigned)p->config.default_private_poll_interval_in_sco : 0u,
      p ? (unsigned)p->config.short_private_poll_interval_in_sco : 0u,
      (unsigned)poll, (unsigned)poll_sco, (unsigned)duration, (unsigned)tpoll,
      (unsigned)tws_hdl, (unsigned)linkid);

  /* If units are 0.625 ms slots: 0x34≈32.5 ms, 0x9c≈97.5 ms, 0x3c≈37.5 ms. */
  if (poll != 0xffff && poll_sco != 0xffff) {
    CROS_LOG(0,
             "[cros_link] %s live≈ %u.%ums acl / %u.%ums in_sco (if slots)", tag,
             (unsigned)((poll * 625u) / 1000u),
             (unsigned)(((poll * 625u) % 1000u) / 100u),
             (unsigned)((poll_sco * 625u) / 1000u),
             (unsigned)(((poll_sco * 625u) % 1000u) / 100u));
  }
}
