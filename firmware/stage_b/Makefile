cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CROS_STAGE_B),1)
obj-y := cros_tws.o cros_besaud_extra.o cros_bt_log.o cros_lat.o cros_sco_probe.o
ifeq ($(CROS_SUSPEND_A2DP),1)
obj-y += cros_a2dp_bridge.o
ccflags-y += -DCROS_SUSPEND_A2DP=1
endif
endif

# v0.3.12: re-enable extra with coexistence (defer + single ping + no-peer retry).
CROS_EXTRA_L2CAP ?= 1
# v0.3.28: SCO/eSCO bud↔bud OPEN/CLOSED probe (no audio on SCO yet).
CROS_SCO_PROBE ?= 1

ccflags-y += \
	$(BT_IF_INCLUDES) \
	$(BT_PROFILES_INCLUDES) \
	-DCROS_EXTRA_L2CAP=$(CROS_EXTRA_L2CAP) \
	-DCROS_SCO_PROBE=$(CROS_SCO_PROBE) \
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
