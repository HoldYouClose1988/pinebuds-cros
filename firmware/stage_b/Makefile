cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CROS_STAGE_B),1)
obj-y := cros_tws.o cros_besaud_extra.o
endif

ccflags-y += \
	$(BT_IF_INCLUDES) \
	$(BT_PROFILES_INCLUDES) \
	-Iapps/cros_tws \
	-Iapps/audioplayers \
	-Iapps/audioplayers/a2dp_decoder \
	-Iservices/audioflinger \
	-Iservices/bt_app \
	-Iservices/bt_app/a2dp_codecs/include \
	-Iservices/app_ibrt/inc \
	-Iservices/app_tws/inc \
	-Iservices/ibrt_core/inc \
	-Iservices/multimedia/audio/codec/sbc/inc \
	-Iservices/multimedia/audio/codec/sbc/src/inc \
	-Iplatform/hal \
	-Iplatform/drivers/bt \
	-Iutils/cqueue

ifeq ($(ANC_APP),1)
ccflags-y += -Iapps/anc/inc
endif
