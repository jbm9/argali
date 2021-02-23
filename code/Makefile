PROJECT = schmancy_blink
BUILD_DIR = bin

SHARED_DIR = src/
CFILES = main.c syscalls.c tamo_state.c leds.c buttons.c
# AFILES += api-asm.S

# Note that OOCD_FILE isn't optional in this framework, which is
# different than the libopencm3 default.
DEVICE=stm32f767zit6u
OOCD_FILE = board/stm32f7discovery.cfg
OOCD_INTERFACE=stlink-v2-1
OOCD_PORT=4444

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
OPENCM3_DIR=./libopencm3/


include $(OPENCM3_DIR)/mk/genlink-config.mk
include libopencm3_rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

PATHS = $(patsubst %,$(SHARED_DIR)/%, . $(CFILES))

openocd-daemon:
	$(OOCD) -f $(OOCD_FILE)

include host_side.mk
