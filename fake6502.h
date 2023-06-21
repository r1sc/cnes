#pragma once
void nmi6502();
void irq6502();
void step6502();
void reset6502();
extern bool hold_cpu;

extern uint32_t clockticks6502;