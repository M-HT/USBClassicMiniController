PROJECT = USBClassicMiniController
BUILD_DIR = bin

CFILES = usbhid.c

DEVICE=stm32f103c8t6
OOCD_FILE = board/stm32f103c8_blue_pill.cfg

OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
