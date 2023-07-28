#ifndef _PPU_H_
#define _PPU_H_

#include <stdint.h>

typedef union {
	struct {
		unsigned int coarse_x_scroll : 5;
		unsigned int coarse_y_scroll : 5;
		unsigned int horizontal_nametable : 1;
		unsigned int vertical_nametable : 1;
		unsigned int fine_y_scroll : 3;
	};
	uint16_t value : 15;
} VRAM_Address_t;

typedef struct {
	uint8_t palette[32];
	VRAM_Address_t T, V;
	uint8_t fine_x_scroll;
	uint8_t ppudata_buffer;
} ppu_state_t;

extern ppu_state_t PPU_state;
extern size_t cpu_timer;
extern int scanline;
extern int dot;
void ppu_reset();

#endif