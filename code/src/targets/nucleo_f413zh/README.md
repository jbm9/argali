# Nucleo F413ZH Target

This document is a rundown of the resources used within the Nucleo
F413ZH target.  When adding target-specific functionality, this is the
first stop to cross-reference what is in use and what is available.
Any peripherals, pins, etc used should be documented here, and
maintained religiously, or you will come to regret it later on.

## Vendor documents

**RM0430 Revision 8: Reference manual for the STM32F413/423**

Reference manual, explaining all the peripherals.  1324 pages of good
information.  This is referenced heavily throughout the documentation
in this target.

**DS11581 Revision 6: STM32F413xG/H Datasheet**

AKA DocID029162 rev 6.

This contains the pin mappings, memory mappings, and electrical
characteristics for the STM32F413ZH on this devboard.

A key thing here is the Alternate Functions table in section 4.9,
pp67-73.  This covers every pin along with its AFs.  It's worth noting
that each AF is assigned to a specific "Port", so all AF2s have to do
with TIM3/4/5.

**DocID029621 Revision 3: STM32F413xG/H Errata sheet**

This is a list of all the bugs and gotchas in the silicon (that ST is
aware of and/or willing to admit to).  Make sure to check if a newer
version of this exists every so often, as it's a key document to stay
on top of.

**PM0214 Revision 10: Programming manual for STM32F3 and STM32F4**

This contains some documentation on a few core peripherals that are
missing from RM0430.  The ones that you probably care about are
SysTick and NVIC stuff, but it also includes the MPU and FPU.

Aside from those, this is mostly just the ARM instruction set, along
with some info about the Cortex-M4.  If you actually need this
document, you seem to have a compiler problem.  Stop what you're
doing, check for typos in your control logic, and if you find none,
call it a day and sleep on it.  You'll find your typo in the morning.

**DB1371 Nucleo-144 Databrief**

This exists.  It is not helpful, but it exists.

**UM1974 Revision 8: STM32 Nucleo-144 User Manual (MB1137)**

Note that this is For the MB1137 board.  We used an as-shipped Rev B
in the development of TamoDevBoard, but I don't see things changing
much revision to revision.  If you start changing option jumpers,
though, all bets are off.

Pinout is in Table 16, pp48-51.

Mechanicals are a tire fire, sorry.  Look upon p14 and weep.


## System Clock

See system_clock.c as the source of truth for this.  By default, this
should be configured for HSE 84MHz:

```
const struct rcc_clock_scale rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_END] = {
	{ /* 84MHz */
		.pllm = 8,
		.plln = 336,
		.pllp = 4,
		.pllq = 7,
		.pllr = 0,
		.pll_source = RCC_CFGR_PLLSRC_HSE_CLK,
		.hpre = RCC_CFGR_HPRE_NODIV,
		.ppre1 = RCC_CFGR_PPRE_DIV2,
		.ppre2 = RCC_CFGR_PPRE_NODIV,
		.voltage_scale = PWR_SCALE1,
		.flash_config = FLASH_ACR_DCEN | FLASH_ACR_ICEN |
				FLASH_ACR_LATENCY_2WS,
		.ahb_frequency  = 84000000,
		.apb1_frequency = 42000000,
		.apb2_frequency = 84000000,
	},
```

The clock signal itself is the default MCO from the ST-LINK at 8MHz,
as documented on UM1974 p25.

## Pins

This section shows which pins are in use; you may wish to expand it to
show all the pins which are available as well.  However, note that
there are a *lot* of pins available, and this project doesn't use all
that many of them.  Once you get past the halfway point, it may make
sense to flip this around.

Alternately, you may want to check in an annotated copy of the
Alternate Functions table from the Datasheet, with each pin
highlighted as it gets used by the team, along with which AF it's in.
Unfortunately, the analog IO functionalities (such as ADC and DAC) use
a separate control from the AFs, so you'll have to stash them
someplace.  AF13 appears to be unused, so there?

In any case, the pin usage for TamoDevBoard is low enough that we'll
enumerate what's in use.

### Baseline pin usage

The only pins available to us on this target are the ones broken out
in the Nucleo144 PCB.  Normally, we would want to explicitly list what
pins are used for what, but, in this case, it seems reasonable to
begin with it framed thusly.

### Pins used by port

#### GPIOA

Port A is used as follows:

| Pin  | Subsystem | Notes                   |
|------|-----------|-------------------------|
| PA0  | ADC       | ADC Input  (CN10.29)    |
| PA2  | ADC       | ADC Input  (CN10.11)    |
| PA4  | DAC       | DAC Output (CN7.17)     |


#### GPIOB

Port B is used as follows:

| Pin  | Subsystem | Notes                   |
|------|-----------|-------------------------|
| PB0  | LEDs      | Green LED (Nucleo144)   |
| PB7  | LEDs      | Blue LED (Nucleo144     |
| PB14 | LEDs      | Red LED (Nucleo144)     |


#### GPIOC

Port C is used as follows:

| Pin  | Subsystem | Notes                   |
|------|-----------|-------------------------|
| PC13 | Buttons   | User button (Nucleo144) |


#### GPIOD

Port D is used as follows:

| Pin  | Subsystem | Notes                   |
|------|-----------|-------------------------|
| PD5  | Console   | TX, AF7 (CN9.6)         |
| PD8  | Console   | TX, AF7 (Nucleo144)     |
| PD9  | Console   | RX, AF7 (Nucleo144)     |


## Clocks

System Clock gets its own section above, at the very start of this
document.


## Timers

### Timer 2

Used to drive the DAC peripheral.  Hard to use anywhere else, as the
DAC needs to twiddle the OC thresholds repeatedly.

### Timer 3

Timer 3 is used for the ADC peripheral.

## DMA peripherals

### DMA 1

DMA 1 Stream 1 is used for USART RX.

DMA 1 Stream 5 is used by DAC.

DMA 1 Stream 6 is used for the dump console TX.

### DMA 2

DMA 2 Stream 0 is used by ADC.

## ADC peripherals

ADC1 is used for ADC input.

There are some guidelines and limits laid out in the Datasheet, on
pp144-150.  Nothing seems to be unusual in it, just the usual best
practices, but it does form a good checklist for review.

For more information on the specifics of the SAR ADC architecture
found in STM32s, check out AN2834.  It doesn't cover the STM32F4, but
it does include a good intro to the SAR architecture, as well as talk
about the sampling rate formulas needed.

We clock this with Timer 3.

Input pins are:
* input 0: PA0, CN10.29 on the Nucleo144
* input 2: PA2, CN10.11 on the Nucleo144

You may wish to add more of these to allow yourself more analog
channels for either your application or your EOL testing.  With two in
there, you should be well-equipped to add more.

For more information on which pins go to what ADC input ports, see the
giant pin definition table in the datasheet (aka DocID029162) Rev6,
pp50-64.  You will find these in the "Additional functions" column, as
`ADC1_INx`.

## DAC peripherals

DAC 1 Channel 1 is used by the DAC subsystem to output data via DMA.
See the comments in dac.c, or in the doxygen docs, see section
[nucleo_f413zh_dac](@ref nucleo_f413zh_dac).

The DAC is clocked with Timer 2

## USART peripherals

USART3 is used by the console.

We use DMA to pull in data for RX, but TX is still blocking.

USART2 is a debug console, where stuff gets dumped to.
