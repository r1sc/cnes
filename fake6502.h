#pragma once
void nmi6502();
void irq6502();
void step6502();
void reset6502();
extern bool hold_clock;

extern size_t clockticks6502;