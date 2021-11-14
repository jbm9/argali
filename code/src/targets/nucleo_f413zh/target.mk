# This is where target-specific Makefile options go, including any
# code that's specialized per platform.

DEVICE=stm32f413zht6u
OOCD_FILE = board/st_nucleo_f4.cfg
OOCD_INTERFACE=stlink-v2-1

CFILES += $(TDIR)/system_clock.c $(TDIR)/leds.c $(TDIR)/buttons.c $(TDIR)/console.c $(TDIR)/dac.c $(TDIR)/adc.c $(TDIR)/timer.c $(TDIR)/dma.c

# AFILES += api-asm.S
