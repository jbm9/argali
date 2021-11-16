#pragma once

/**
 * \file dma.h
 * \brief Header for DMA driver (Nucleo F767ZI)
 */

/**
 * \addtogroup nucleo_f767zi_dma
 * \{
 */

#ifdef TEST_UNITY
#warning Building without hardware DMA support
#else

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dac.h>
#include <libopencm3/stm32/dma.h>

#endif


typedef struct dma_settings {
  uint32_t dma;     //!< DMA peripheral to use (DMA1)
  uint32_t stream;   //!< DMA stream to use (DMA1_STREAM5)
  uint32_t channel; //!< Channel of the stream to select (DMA_SxCR_CHSEL_7)
  uint32_t priority; //!< Priority (DMA_SxCR_PL_LOW)

  uint32_t direction; //< Direction of transfer (DMA_SxCR_DIR_PERIPHERAL_TO_MEM_

  uint32_t paddr; //!< Peripheral address (cast to uint32_t)
  uint32_t peripheral_size; //!< Size of the peripheral memory (DMA_SxCR_PSIZE_8BIT)

  uint32_t buf;   //!< Target memory address  (cast to uint32_t)
  uint16_t buflen; //!< Length of target memory (as uint16_t)
  uint32_t mem_size; //!< Size of target word (DMA_SxCR_MSIZE_8BIT)

  bool circular_mode; //!< If true, enable circular mode
  bool double_buffer; //!< If true, enable double buffer mode (we split your buffer for you)

  bool transfer_complete_interrupt;  //!< If true, enable TCIF flag
  bool enable_irq; //!< Whether or not to enable the NVIC IRQ here
  uint8_t irqn; //!< The IRQ to enable with nvic_enable_irq (NVIC_DMA1_STREAM5_IRQ)

  bool enable_stream; //!< Whether or not to enable the stream

} dma_settings_t;

void dma_setup(dma_settings_t *);

/** \} */
