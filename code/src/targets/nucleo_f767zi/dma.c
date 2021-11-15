/**
 * \file dma.c
 * \brief Nucleo F767ZI DMA interface implementation
 */


/**
 * \defgroup nucleo_f767zi_timer DMA driver (Nucleo F767ZI)
 *
 * \addtogroup nucleo_f767zi_dma
 * \{
 * \ingroup nucleo_f767zi
 *
 * This is a thin layer to abstract out the DMA setup for the ADC and
 * DAC peripheral drivers.  It is laughably incomplete for the general
 * case, but suffices for our needs.
 *
 * DMA Details: Chapter 8 of RM0410, p245
 *
 * DMA Channel/Stream request mapping table is p249.
 * ADC1 is DMA2 Channel 0 stream 1 or 4
 * DAC1 is DMA1 Channel 5 stream 7
 *
 * Circular mode is p254: 8.3.9: set the CIRC bit in DMA_SxCR, and
 * need to make sure the MBURST is one of 4/8/16.  Also need the
 * memory and peripheral record sizes to be simple power of two or
 * four {1/4,1/2,1,2,4}.

 * Double-buffer mode is p254: 8.3.10.  DBM set in DMA_SxCR register.
 * This automatically turns on circular for you (ignoring CIRC).  At
 * the end of each transaction, the buffer pointers are swapped.
 * Relevant App Note is AN4031.  AN4031 pp14&5 1.2 is a schematization
 * of setting up a DMA transfer.
 *
 * Note that any time we interrupt the DMA peripheral, we need to
 * reconfigure it fully before restarting.
 */

#include "dma.h"


/**
 * \brief Brings up the DMA with a given configuration.
 *
 * Note that this treats your buffer as a contiguous unit for two
 * half-length buffers, and splits it in half for you.
 *
 * This does a full reset of the peripheral, then follows the flow
 * found in RM0410r4 p2, 9.3.18 "Stream configuration procedure"
 *
 */
void dma_setup(dma_settings_t *s) {

  // 1. Set to disabled
  dma_stream_reset(s->dma, s->stream); // Sets EN = 0, resets DMA

  // 2. Set the port register address
  dma_set_peripheral_address(s->dma, s->stream, s->paddr);

  // 3. Set the memory address in DMA_SxMA0R (and SxMA1R)
  dma_set_memory_address(s->dma, s->stream, s->buf);

  if (s->double_buffer)
    dma_set_memory_address_1(s->dma, s->stream, s->buf+(s->buflen/2));

  // 4. Configure the data items to transferg
  dma_set_number_of_data(s->dma, s->stream,
                         (s->double_buffer ? s->buflen/2 : s->buflen));

  // 5. Select the channel
  dma_channel_select(s->dma, s->stream, s->channel);

  // 6. If the peripheral is a flow controller, set that up (this isn't)

  // 7. Configure priority
  dma_set_priority(s->dma, s->stream, s->priority);

  // 8. Configure FIFO (not using one)
  dma_enable_direct_mode(s->dma, s->stream);  // Don't use the FIFO

  // 9. Configure the transfer direction etc (see document)
  dma_set_transfer_mode(s->dma, s->stream, s->direction);

  dma_enable_memory_increment_mode(s->dma, s->stream);
  dma_set_peripheral_size(s->dma, s->stream, s->peripheral_size);
  dma_set_memory_size(s->dma, s->stream, s->mem_size);

  if (s->circular_mode)
    dma_enable_circular_mode(s->dma, s->stream);

  if (s->double_buffer)
    dma_enable_double_buffer_mode(s->dma, s->stream);

  if (s->transfer_complete_interrupt)
    dma_enable_transfer_complete_interrupt(s->dma, s->stream);
  else
     dma_disable_transfer_complete_interrupt(s->dma, s->stream);

  if (s->enable_irq)
    nvic_enable_irq(s->irqn);

  // 10. Activate the stream
  dma_enable_stream(s->dma, s->stream);
}


/** \} */
