cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CROS_STAGE_B),1)
obj-y := cros_tws.o cros_besaud_extra.o cros_bt_log.o cros_lat.o cros_a2dp_bridge.o
endif

# v0.3.12: re-enable extra with coexistence (defer + single ping + no-peer retry).
CROS_EXTRA_L2CAP ?= 1

ccflags-y += \
	$(BT_IF_INCLUDES) \
	$(BT_PROFILES_INCLUDES) \
	-DCROS_EXTRA_L2CAP=$(CROS_EXTRA_L2CAP) \
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
