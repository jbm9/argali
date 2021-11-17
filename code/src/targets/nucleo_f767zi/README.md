# Nucleo F767ZI Target

This document is a rundown of the resources used within the Nucleo
F767ZI target.  When adding target-specific functionality, this is the
first stop to cross-reference what is in use and what is available.
Any peripherals, pins, etc used should be documented here, and
maintained religiously, or you will come to regret it later on.

## Vendor documents

All of these can be found squirreled away somewhere on the [STM32F7x7
Documentation
Page](https://www.st.com/en/microcontrollers-microprocessors/stm32f7x7.html#documentation).

**RM0410 Revision 4: Reference manual for the STM32F76xxx/STM32F77xxx**

Reference manual, explaining all the peripherals.  1954 pages of joy.
This is referenced heavily throughout the documentation in this
target.

**DS11532 Revision 7: STM32F76{5,6,8A,9}xx Datasheet**

This contains the pin mappings, memory mappings, and electrical
characteristics for the STM32F767ZI on this devboard.

A key thing here is the Alternate Functions table 13 pp89-102.  This
covers every pin along with its AFs.  It's worth noting that each AF
is assigned to a specific "Port", so all AF2s have to do with
TIM3/4/5.

**ES0334 Revision 7: STM32F7[67]xxx Errata Sheet**

This is a list of all the bugs and gotchas in the silicon (that ST is
aware of and/or willing to admit to).  Make sure to check if a newer
version of this exists every so often, as it's a key document to stay
on top of.

**PM0253 Revision 10: Programming manual for STM32H7 and STM32M7**

This contains some documentation on a few core peripherals that are
missing from RM0410.  The ones that you probably care about are
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

Pinout is in Table 16, pp60-64.

Mechanicals are a tire fire, sorry.  Look upon p14 and weep.


## System Clock

See system_clock.c as the source of truth for this.  By default, this
should be configured for HSE 216MHz:

```
{ /* 216MHz */
		.plln = 432,
		.pllp = 2,
		.pllq = 9,
		.hpre = RCC_CFGR_HPRE_NODIV,
		.ppre1 = RCC_CFGR_PPRE_DIV4,
		.ppre2 = RCC_CFGR_PPRE_DIV2,
		.vos_scale = PWR_SCALE1,
		.overdrive = 1,
		.flash_waitstates = 7,
		.ahb_frequency = 216000000,
		.apb1_frequency = 54000000,
		.apb2_frequency = 108000000,
	},``
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

**Relevant Erratum!  2.2.6 PC13 signal transitions disturb LSE**

If we try to use RTC, this user button may possibly interfere.  The
specific text suggests that it's actually toggling PC13 from the MCU
side that messes up the LSE.

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

### Timer 4

Timer 4 is used for the ADC peripheral.

## DMA peripherals



### DMA 1

DMA 1 Stream 1 is used by the USART RX path.

DMA 1 Stream 5 is used by DAC.

DMA 1 Stream 6 is used by the dump console for TX

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

We clock this with Timer 4.  This differs from the F413ZH
implementation, which uses timer 2.

**Relevant erratum!  2.2.1 in ES0334r7** points out that you can get
noise on the ADC from VDD, and offers recommendations if workarounds
are needed.  It is unclear if we care about this in our application.

## DAC peripherals

DAC 1 Channel 1 is used by the DAC subsystem to output data via DMA.
See the comments in dac.c, or in the doxygen docs, see section
[nucleo_f767zi_dac](@ref nucleo_f767zi_dac).

The DAC is clocked with Timer 2

**Relevant erratum: 2.6.2: DMA request not automatically cleared by
clearing DMAEN**

It looks like we're going to need to apply this workaround for
starting and stopping DAC.  In particular, the F767 DTMF decoder is
having a lot harder time than the F413 version.  This may be why.

## USART peripherals

USART2 is used by the dump console

USART3 is used by the console.

The receive side uses DMA, while transmit is still blocking.

