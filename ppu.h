#pragma once
#include <stdint.h>

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} pixformat_t;

extern pixformat_t framebuffer[256 * 240];
extern int scanline;
extern int dot;


uint8_t cpu_ppu_bus_read(uint8_t address);
void cpu_ppu_bus_write(uint8_t address, uint8_t value);
void tick();