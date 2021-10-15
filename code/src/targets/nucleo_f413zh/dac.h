#pragma once

#include <stdbool.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

/**
 * \file dac.h
 * \brief Header file for DAC driver (Nucleo F413ZH)
 */

/**
 * \addtogroup nucleo_f413zh_dac
 * \{
 *
 */

#define DAC_UNIT DAC1 //!< Which DAC to use
#define DAC_CHANNEL DAC_CHANNEL1 //!< Which DAC Channel to use
#define DAC_TRIGGER_SEL DAC_CR_TSEL1_T2 //!< Trigger selector for the DAC

#define DAC_PORT GPIOA //!< GPIO port the DAC output is on
#define DAC_PIN GPIO4   //!< GPIO PIN the DAC output is on (CN7.17, opposite D9)
#define DAC_GPIO_CLOCK RCC_GPIOA //!< Clock for the DAC's GPIO output

#define DAC_TIMER TIM2           //!< Timer used by DAC's DMA to advance samples
#define DAC_TIMER_CLOCK RCC_TIM2 //!< Clock to drive DAC DMA sample advance timer
#define DAC_TIMER_OUTPUT TIM_OC1 //!< Edge of the timer to use to advance samples
#define DAC_TIMER_RESET RST_TIM2 //!< Reset line for DAC's DMA timer

#define DAC_DMA_CTRL DMA1 //!< DAC's DMA controller
#define DAC_DMA_STREAM DMA_STREAM5 //!< DAC's DMA stream
#define DAC_DMA_CHSEL DMA_SxCR_CHSEL_7 //!< DAC's DMA channel
#define DAC_DMA_CLOCK RCC_DMA1 //!< Clock for DAC's DMA

typedef struct {
  bool initialized;
  uint8_t *buf;
  uint16_t buflen;
} dac_state_t;


void dac_setup(void);

int dac_register_buf(uint8_t *, uint16_t);
int dac_start(int, uint32_t);
int dac_stop(void);

/** \} */ // End doxygen group
