#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "buttons.h"
#include "leds.h"
#include "syscalls.h"
#include "tamo_state.h"


/** Configures USART3 at 115200 8N1
 */
static void usart_setup(void) {
  // Enable USART3 clocks
  rcc_periph_clock_enable(RCC_GPIOD); // USART TX pin
  rcc_periph_clock_enable(RCC_USART3);

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

  usart_setup();
  printf("Hello console yo!\n");
  led_setup();
  button_setup();

  current_time = 0;
  tamo_state_init(&tamo_state, current_time);

  printf("0\n");
  led_red_on();
  printf("1\n");
  _delay_ms(100);
  printf("2\n");
  led_blue_on();
  _delay_ms(100);
  led_green_on();
  _delay_ms(100);
  led_red_off();
  _delay_ms(100);
  led_blue_off();
  _delay_ms(100);
  led_green_off();
  printf("9\n");
  while (1) {
    // Run this loop at about 10Hz, instead of using ISRs like a real programmer

    for (j = 0; j < 10; j++) {
      bool user_present = button_poll();

      if (tamo_state_update(&tamo_state, current_time, user_present)) {
        printf("Transition to %s: %d\n",
               tamo_emotion_name(tamo_state.current_emotion), user_present);
      }
      switch (tamo_state.current_emotion) {
      case TAMO_LONELY: // blink red at 5Hz when lonely
	led_blue_off();
        led_red_on();
        break;

      case TAMO_HAPPY: // Solid blue when happy
	led_red_off();
	led_blue_on();
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
