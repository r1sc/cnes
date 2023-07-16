#pragma once
#include <stdint.h>

void apu_tick(uint16_t scanline);
void apu_write(uint16_t address, uint8_t value);