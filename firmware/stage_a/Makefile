cur_dir := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CROS_STAGE_A),1)
obj-y := cros_loopback.o
endif

ccflags-y += \
	-Iapps/cros_loopback \
	-Iapps/audioplayers \
	-Iservices/audioflinger \
	-Iplatform/hal \
	-Iutils/cqueue

ifeq ($(ANC_APP),1)
ccflags-y += -Iapps/anc/inc
endif
