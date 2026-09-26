cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CROS_STAGE_B),1)
obj-y := cros_tws.o cros_besaud_extra.o cros_bt_log.o cros_lat.o cros_sco_probe.o \
	cros_sco_voice_bridge.o
ifeq ($(CROS_SUSPEND_A2DP),1)
obj-y += cros_a2dp_bridge.o
ccflags-y += -DCROS_SUSPEND_A2DP=1
endif
endif

# v0.3.12: re-enable extra with coexistence (defer + single ping + no-peer retry).
# v0.3.37 alone mode skips opening extra even if this is 1.
CROS_EXTRA_L2CAP ?= 1
# v0.3.34: SCO probe ON again (settle-after-READY). Keep poor-master refuse.
CROS_SCO_PROBE ?= 1
# v0.3.35: last §K probe — slave also issues sco_open_link.
CROS_SCO_SLAVE_OPEN ?= 1
# v0.3.37: leave peer SCO up without extra L2CAP (next coexistence proof).
CROS_SCO_ALONE ?= 1
# v0.3.39: on OPENED, start HFP CVSD voice player against peer SCO (media probe).
CROS_SCO_MEDIA ?= 1
# v0.3.41: default HFP/SCO playback level on good side (0..15).
CROS_SCO_HFP_VOL ?= 13
# v0.3.33: allow POOR/TX when it is IBRT master (default refuse — known crash).
CROS_ALLOW_POOR_MASTER ?= 0

ccflags-y += \
	$(BT_IF_INCLUDES) \
	$(BT_PROFILES_INCLUDES) \
	-DCROS_EXTRA_L2CAP=$(CROS_EXTRA_L2CAP) \
	-DCROS_SCO_PROBE=$(CROS_SCO_PROBE) \
	-DCROS_SCO_SLAVE_OPEN=$(CROS_SCO_SLAVE_OPEN) \
	-DCROS_SCO_ALONE=$(CROS_SCO_ALONE) \
	-DCROS_SCO_MEDIA=$(CROS_SCO_MEDIA) \
	-DCROS_SCO_HFP_VOL=$(CROS_SCO_HFP_VOL) \
	-DCROS_ALLOW_POOR_MASTER=$(CROS_ALLOW_POOR_MASTER) \
	-Iapps/cros_tws \
	-Iapps/audioplayers \
	-Iapps/audioplayers/a2dp_decoder \
	-Iservices/audioflinger \
	-Iservices/bt_app \
	-Iservices/bt_app/a2dp_codecs/include \
	-Iservices/app_ibrt/inc \
	-Iservices/app_tws/inc \
	-Iservices/ibrt_core/inc \
	-Iservices/ibrt_ui/inc \
	-Iservices/multimedia/audio/codec/sbc/inc \
	-Iservices/multimedia/audio/codec/sbc/src/inc \
	-Iservices/tota \
	-Iplatform/hal \
	-Iplatform/drivers/bt \
	-Iutils/cqueue

ifeq ($(ANC_APP),1)
ccflags-y += -Iapps/anc/inc
endif
