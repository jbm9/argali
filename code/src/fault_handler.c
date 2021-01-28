

void hard_fault_handler(void) {
  __asm(
    "MRS r0, MSP\n" // Default to the Main Stack Pointer
    "MOV r1, lr\n"  // Load the current link register value
    "MOVS r2, #4\n" // Load constant 4
    "TST r1, r2\n"  // Test whether we are in master or thread mode
    "BEQ base_fault_handler\n" // If in master mode, MSP is correct.
    "MRS r0, PSP\n" // If we weren't in master mode, load PSP instead
    "B base_fault_handler"); // Jump to the fault handler.
}


// Core ARM interrupt names. These interrupts are the same across the family.
static const char *system_interrupt_names[16] = {
    "SP_Main",      "Reset",    "NMI",        "Hard Fault",
    "MemManage",    "BusFault", "UsageFault", "Reserved",
    "Reserved",     "Reserved", "Reserved",   "SVCall",
    "DebugMonitor", "Reserved", "PendSV",     "SysTick"};

void base_fault_handler(uint32_t stack[]) {
  // The implementation of these fault handler printf methods will depend on
  // how you have set your microcontroller up for debugging - they can either
  // be semihosting instructions, write data to ITM stimulus ports if you
  // are using a CPU that supports TRACESWO, or maybe write to a dedicated
  // debug UART
  fault_handler_printf("Fault encountered!\n");
  static char buf[64];
  // Get the fault cause. Volatile to prevent compiler elision.
  const volatile uint8_t active_interrupt = arm::scb::ICSR & 0xFF;
  // Interrupt numbers below 16 are core system interrupts, we know their names
  if (active_interrupt < 16) {
    sprintf_(buf, "Cause: %s (%u)\n", system_interrupt_names[active_interrupt],
             active_interrupt);
  } else {
    // External (user) interrupt. Must be looked up in the datasheet specific
    // to this processor / microcontroller.
    sprintf_(buf, "Unimplemented user interrupt %u\n", active_interrupt - 16);
  }
  fault_handler_printf(buf);

  fault_handler_printf("Saved register state:\n");
  dump_registers(stack);
  __asm volatile("BKPT #01");
  while (1) {
  }
}
