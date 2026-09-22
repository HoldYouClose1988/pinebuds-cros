cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CROS_STAGE_B),1)
obj-y := cros_tws.o
endif

ccflags-y += \
	-Iapps/cros_tws \
	-Iapps/audioplayers \
	-Iservices/audioflinger \
	-Iservices/bt_app \
	-Iservices/app_ibrt/inc \
	-Iservices/app_tws/inc \
	-Iplatform/hal \
	-Iutils/cqueue

ifeq ($(ANC_APP),1)
ccflags-y += -Iapps/anc/inc
endif
