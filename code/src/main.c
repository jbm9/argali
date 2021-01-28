#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "syscalls.h"
#include "tamo_state.h"

// LD1: green: PB0
// LD2: blue: : PB7
// LD3: red : PB14

// usr button: PC13

static void clock_setup(void) {
  // Enable LEDs
  rcc_periph_clock_enable(RCC_GPIOB);

  // Enable button
  rcc_periph_clock_enable(RCC_GPIOC);

  // Enable USART3
  rcc_periph_clock_enable(RCC_GPIOD); // USART TX pin
  rcc_periph_clock_enable(RCC_USART3);
}

/** Configures USART3 at 115200 8N1
 */
static void usart_setup(void) {
  // Connect USART3 TX/RX pins via AF
  gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);
  gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO7);
  gpio_set_af(GPIOD, GPIO_AF7, GPIO8 | GPIO9);

  // Now actually set up the USART peripheral
  usart_set_baudrate(USART3, 115200);
  usart_set_databits(USART3, 8);
  usart_set_stopbits(USART3, USART_STOPBITS_1);
  usart_set_mode(USART3, USART_MODE_TX_RX);
  usart_set_parity(USART3, USART_PARITY_NONE);
  usart_set_flow_control(USART3, USART_FLOWCONTROL_NONE);

  // And ... engage!
  usart_enable(USART3);
}

/** Configures our LED GPIOs
 *
 */
static void gpio_setup(void) {
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);

  gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO13);
}

/** Toggle the red LED
 */

static void led_red_toggle(void) { gpio_toggle(GPIOB, GPIO14); }

/** Toggle the blue LED
 */
static void led_blue_toggle(void) { gpio_toggle(GPIOB, GPIO7); }

#define CLOCKS_PER_MS 10000

static void _delay_ms(uint16_t ms) {
  //! \todo actually use the clock speed for delays
  for (uint32_t i = 0; i < ms * CLOCKS_PER_MS; i++)
    __asm__("NOP");
}

int main(void) {
  uint32_t i = 0;
  uint32_t j = 0;
  uint8_t c = 0;

  uint32_t current_time; //! \todo We still need to hook up the RTC.
  tamo_state_t tamo_state;

  clock_setup();
  gpio_setup();
  usart_setup();

  current_time = 0;
  tamo_state_init(&tamo_state, current_time);

  printf("Hello console!\n");

  while (1) {
    // Run this loop at about 10Hz, instead of using ISRs like a real programmer

    for (j = 0; j < 10; j++) {
      uint16_t button_state = gpio_get(GPIOC, GPIO13);
      bool user_present = (button_state != 0);

      if (tamo_state_update(&tamo_state, current_time, button_state)) {
        printf("Transition to %s: %d\n",
               tamo_emotion_name(tamo_state.current_emotion), button_state);
      }
      switch (tamo_state.current_emotion) {
      case TAMO_LONELY: // blink red at 5Hz when lonely
        gpio_clear(GPIOB, GPIO7);
        led_red_toggle();
        break;

      case TAMO_HAPPY: // Solid blue when happy
        gpio_clear(GPIOB, GPIO14);
        gpio_set(GPIOB, GPIO7);
        break;

      case TAMO_BORED: // Blink blue at 2Hz when bored
        if (j % 5 == 0) {
          led_blue_toggle();
        }
        break;
      case TAMO_UNKNOWN:
      default:
        led_blue_toggle();
        led_red_toggle();
        break;
      }
      _delay_ms(100);
    }
    // Now increment our second
    current_time++;
  }
}
