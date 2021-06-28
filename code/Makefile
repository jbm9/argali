# The filename for the result binary
PROJECT_NAME = schmancy_blink

# Default target unless one is given.
TARGET ?= nucleo_f767zi
# Note that TDIR is *relative* to SHARED_DIR
TDIR = src/targets/$(TARGET)/

CFILES = main.c syscalls.c tamo_state.c logging.c

######################################################################
# You shouldn't have to edit anything below here.

BUILD_DIR = bin/$(TARGET)

# This is a bit of a kludge to get libopencm3's Makefile to put our
# binaries in the target-appropriate subdirectory.
PROJECT = $(BUILD_DIR)/$(PROJECT_NAME)

SHARED_DIR = src/

# Anything that is hardware-specific should be defined in a Makefile
# snippet for that hardware.  This includes appending things to CFILES
# that need to be built per-target, such as the drivers for LEDs etc.

include src/targets/$(TARGET)/target.mk
#main.o: $(CFILES:%.c=%.o)

OOCD_PORT=4444

VPATH += $(SHARED_DIR)
VPATH += $(TDIR)

INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
INCLUDES += $(patsubst %,-I%, . $(TDIR))

OPENCM3_DIR=./libopencm3/

include $(OPENCM3_DIR)/mk/genlink-config.mk
include libopencm3_rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

openocd-daemon:
	$(OOCD) -f $(OOCD_FILE)

include host_side.mk
