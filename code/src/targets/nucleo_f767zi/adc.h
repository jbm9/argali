#pragma once

void adc_setup(uint8_t *, const uint32_t);
extern volatile uint32_t adc_isrs;
void adc_pause(void);
void adc_unpause(void);
