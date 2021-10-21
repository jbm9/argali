/*
  RM0410 pp438-486

  For 12b resolution, we need 15 ADCCLK cycles to convert For 8b,this
  is only 11.

  Big caveat about overflow and DMA interactions on p453 (15.8.1 Using
  the DMA)

  ADC_CR2 DDS bit is key to continuing the samples.

  Looks like we want DMA mode 3, which will give two simultaneous 8b
  values per DMA xfer.

  If we go to triple DMA, probably want to switch back to Mode 1, as
  the juggling of values in Modes 2 and 3 are crazypants.

  Probably want to add an OVRIE handler (p469) at some point, to keep
  track of any problems with ADC in the device.

  https://embedds.com/multichannel-adc-using-dma-on-stm32/ -- STM32F1, unfortunately (it has different ADC periphs)

  Using channel 0 of ADC1, which is PA0.  Will want to add channel 1
  of ADC2 for current monitoring in the final application, which is at
  PA1.

  DMA Stream 2 channel 0 stream 0 is ADC1 (RM0410r4 p249 Table 28)

  Ah, found a good example to work off of:
  https://github.com/JonasNorling/guitarboard/blob/b3e7b9216327e9e91580e97f254aaab5a42a5b1c/fw/src/target/platform-stm32.c

  https://github.com/ksarkies/ARM-Ports/blob/4f1584b8cfe47ac615ce31fd8645255f27151aca/test-libopencm3-stm32f4/adc-dma-stm32f4discovery.c
*/

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include "adc.h"


volatile uint32_t adc_isrs = 0;

static void adc_setup_clocks(void) {
  rcc_periph_clock_enable(RCC_GPIOA);

  rcc_periph_clock_enable(RCC_ADC1);

  rcc_periph_clock_enable(RCC_DMA2); // DMA2 stream 0
  rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_DMA2EN);
}

static void adc_setup_gpio(void) {
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0); // PA0 -- CN10.29
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO1);  // Going to want this soon

}

static void adc_setup_adc(void) {
  uint8_t channels[1] = {0};
  const uint8_t n_channels = 1;

  nvic_enable_irq(NVIC_ADC_IRQ);

  adc_power_off(ADC1); // Turn off ADC to configure sampling
  adc_enable_scan_mode(ADC1);
  adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_112CYC);
  adc_set_clk_prescale(ADC_CCR_ADCPRE_BY8);
  adc_set_continuous_conversion_mode(ADC1);
  adc_set_resolution(ADC1, ADC_CR1_RES_8BIT);
  adc_set_dma_continue(ADC1);
  adc_set_regular_sequence(ADC1, n_channels, channels);
  adc_enable_dma(ADC1);
  adc_enable_overrun_interrupt(ADC1);

  adc_power_on(ADC1);  // Re-power ADC peripheral
}

static void adc_setup_dma(uint8_t *buff, const uint32_t buflen) {
  // Set up DMA, following RM0410r4 p263, 8.3.18 "Stream configuration procedure"
  // 1. Set to enabled: NB, not checking for EN to be 0 before!
  dma_stream_reset(DMA2, DMA_STREAM0);

  // 2. Set the port register address
  dma_set_peripheral_address(DMA2, DMA_STREAM0, (uint32_t) &ADC_DR(ADC1));

  // 3. Set the memory address in DMA_SxMA0R
  dma_set_memory_address(DMA2, DMA_STREAM0, (uint32_t) buff);

  // 4. Configure the data items to transfer
  dma_set_number_of_data(DMA2, DMA_STREAM0, buflen);

  // 5. Select the channel
  dma_channel_select(DMA2, DMA_STREAM0, DMA_SxCR_CHSEL_0);

  // 6. If the peripheral is a flow controller, set that up (this isn't)

  // 7. Configure priority
  dma_set_priority(DMA2, DMA_STREAM0, DMA_SxCR_PL_LOW);

  // 8. Configure FIFO (not using one)
  dma_enable_direct_mode(DMA2, DMA_STREAM0);  // Don't use the FIFO

  // 9. Configure the transfer direction etc (see document)
  dma_set_transfer_mode(DMA2, DMA_STREAM0, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);

  dma_enable_memory_increment_mode(DMA2, DMA_STREAM0);
  dma_set_peripheral_size(DMA2, DMA_STREAM0, DMA_SxCR_PSIZE_8BIT);
  dma_set_memory_size(DMA2, DMA_STREAM0, DMA_SxCR_MSIZE_8BIT);

  dma_enable_circular_mode(DMA2, DMA_STREAM0);
  dma_enable_transfer_complete_interrupt(DMA2, DMA_STREAM0);
  //dma_enable_half_transfer_interrupt(DMA2, DMA_STREAM0);
  nvic_enable_irq(NVIC_DMA2_STREAM0_IRQ);
  // 10. Activate the stream
  dma_enable_stream(DMA2, DMA_STREAM0);
}

void adc_pause(void) {
  //dma_disable_stream(DMA2, DMA_STREAM0);
  //  adc_disable_dma(ADC1);
  rcc_periph_clock_disable(RCC_ADC1);
}

void adc_unpause(void) {
  //  dma_enable_stream(DMA2, DMA_STREAM0);
  //  adc_enable_dma();
  rcc_periph_clock_enable(RCC_ADC1);
}

/**
 * \brief Sets up ADC1 on DMA2 Stream 0 for input
 *
 * \param buff Buffer to write data into
 * \param buflen Number of entries in this buffer
 */
void adc_setup(uint8_t *buff, const uint32_t buflen) {
  adc_setup_clocks();
  adc_setup_gpio();

  adc_setup_adc();

  adc_setup_dma(buff, buflen);

  // And now we can kick off the ADC conversion
  adc_start_conversion_regular(ADC1);
}

void dma2_stream0_isr(void) {
  adc_isrs++;

  if((DMA2_LISR & DMA_LISR_TCIF0) != 0) {
    // Clear this flag so we can continue
    dma_clear_interrupt_flags(DMA2, DMA_STREAM0, DMA_LISR_TCIF0);
  }
}


void adc_isr(void) {
  if (adc_get_overrun_flag(ADC1)) {
    adc_clear_overrun_flag(ADC1);
  }
}
