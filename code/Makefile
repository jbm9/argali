# The filename for the result binary
PROJECT_NAME = schmancy_blink

# Default target unless one is given.
TARGET ?= nucleo_f767zi
# Note that TDIR is *relative* to SHARED_DIR
TDIR = src/targets/$(TARGET)/


# Don't include per-target files here; those belong in the
# $(TDIR)/target.mk file.  This is done so that target code can be
# organized as you see fit, and you don't have to ensure everything
# matches exactly across the many targets you might have.
#
# This should just be files in the "application" code.
CFILES = main.c syscalls.c tamo_state.c logging.c sin_gen.c dtmf.c pi_reciter.c packet.c eol_commands.c

######################################################################
# You shouldn't have to edit anything below here.

BUILD_DIR = bin/$(TARGET)

ARGALI_VERSION = $(shell ./argali_version.sh)

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

# Inject our version string into a cpp #define
CPPFLAGS += -DARGALI_VERSION=$(ARGALI_VERSION)

# We need libm for atan2()
LDLIBS += -lm

include $(OPENCM3_DIR)/mk/genlink-config.mk
include libopencm3_rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

openocd-daemon:
	$(OOCD) -f $(OOCD_FILE)


include host_side.mk
