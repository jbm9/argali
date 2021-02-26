/**
 * \file buttons.c
 * \brief Nucleo F767ZH Button interface implementation
 */

#include "buttons.h"

/**
 * \defgroup nucleo_f413zh_buttons Button input handlers (Nucleo F413ZH)
 *
 * \addtogroup nucleo_f413zh_buttons
 * \{
 * \ingroup nucleo_f413zh
 */

/**
 * \brief Sets up the clock and GPIO pin for our button input
 */
void button_setup(void) {
  rcc_periph_clock_enable(BUTTON_CLOCK);
  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO13);
}

/**
 * \brief Read the current value of the button input
 *
 * \return True if the button is depressed, false otherwise
 */
bool button_poll(void)
{
  return gpio_get(BUTTON_PORT, BUTTON_PIN) ? true : false;
}

/** \} */ // End doxygen group
