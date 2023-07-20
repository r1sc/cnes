#pragma once

#include <stdint.h>
#include <stdbool.h>

void nmi6502();
void irq6502();
void step6502();
void reset6502();
uint8_t read6502(uint16_t address);

extern size_t total_steps_6502;
extern size_t clockticks6502;
extern uint16_t pc;
extern uint8_t sp, a, x, y, status;