# This is where target-specific Makefile options go, including the
# code for the drivers.

DEVICE=stm32f767zit6u
OOCD_FILE = board/stm32f7discovery.cfg
OOCD_INTERFACE=stlink-v2-1

CFILES += $(TDIR)/system_clock.c $(TDIR)/leds.c $(TDIR)/buttons.c $(TDIR)/console.c

# AFILES += api-asm.S
