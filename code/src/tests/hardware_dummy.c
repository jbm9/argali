// A terrible collection of kludges to fake out hardware state without CMock.

#ifndef TEST_UNITY
#error This file should only be used from inside of unity tests!
#else

//////////////////////////////////////////////////////////////////////
//
// Nasty global state to stash calls in; a very poor version of
// mocking, but one without as much infrastructure overhead for now.

// DAC state
uint8_t HW_DAC_setup;      //!< Has the DAC been set up via dac_setup()?
uint16_t HW_DAC_prescaler; //!< DAC timer Prescaler
uint32_t HW_DAC_period;    //!< DAC timer Period
uint8_t *HW_DAC_buf;       //!< DAC Buffer target
uint16_t HW_DAC_buflen;    //!< DAC buffer length
uint8_t HW_DAC_running;    //!< Is the DAC running? (dac_start/dac_stop)

// scb_reset_system state
uint8_t HW_did_reset;  //!< Has a system reset been ordered?

// Packet state
uint32_t HW_packet_count;    //!< Number of packet_send() calls
uint8_t HW_packet_buf[2048]; //!< A copy of the most recent packet submission
uint16_t HW_packet_len;      //!< Length of most recent packet submission
uint8_t HW_packet_addr;      //!< Address of most recent packet submission
uint8_t HW_packet_command;   //!< Command of most recent packet submission


void HW_set_default_state(void) {
  HW_DAC_setup = 0;
  HW_DAC_prescaler = 0;
  HW_DAC_period = 0;
  HW_DAC_buf = NULL;
  HW_DAC_buflen = 0;
  HW_DAC_running = 0;    
  
  HW_did_reset = 0;

  HW_packet_count = 0;
  memset(HW_packet_buf, '*', 2048);
  HW_packet_len = 0;
  HW_packet_addr = 0;
  HW_packet_command = 0;
}


//////////////////////////////////////////////////////////////////////
// DAC dummy
void dac_setup(uint16_t prescaler, uint32_t period, const uint8_t *buf, uint16_t buflen) {
  HW_DAC_setup = 1;
  HW_DAC_prescaler = prescaler;
  HW_DAC_period = period;
  HW_DAC_buf = buf;
  HW_DAC_buflen = buflen;
  HW_DAC_running = 0;    
}

float dac_get_sample_rate(uint16_t prescaler, uint32_t period) {
  return 0.0; // TODO
}

void dac_start(void) {
  HW_DAC_running = 1;
}
void dac_stop(void)  {
  HW_DAC_running = 0;  
}


//////////////////////////////////////////////////////////////////////
// SCB dummy
void scb_reset_system(void) {
  printf("Reset!");
  did_reset = 1;
}

//////////////////////////////////////////////////////////////////////
// Packet state (very close to hardware)

void packet_send(const uint8_t *buf, uint16_t buflen, uint8_t address, uint8_t command) {
  HW_packet_count++;
  memcpy(HW_packet_buf, buf, buflen);
  HW_packet_len = buflen;
  HW_packet_addr = address;
  HW_packet_command = command;
}



#endif
