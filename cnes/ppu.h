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

	struct OAMEntry_t {
		uint8_t y;
		uint8_t tile_index;
		uint8_t attributes;
		uint8_t x;
	} OAM[64];

	union {
		struct {
			unsigned int nametable_address : 2;
			unsigned int vram_address_increment : 1;
			unsigned int sprite_pattern_table_address : 1;
			unsigned int background_pattern_table_address : 1;
			unsigned int sprite_size : 1;
			unsigned int ppu_master_slave : 1;
			unsigned int gen_nmi_vblank : 1;
		};
		uint8_t value;
	} control;

	union {
		struct {
			unsigned int greyscale : 1;
			unsigned int show_background_left : 1;
			unsigned int show_sprites_left : 1;
			unsigned int show_background : 1;
			unsigned int show_sprites : 1;

			unsigned int emphasize_red : 1;
			unsigned int emphasize_green : 1;
			unsigned int emphasize_blue : 1;
		};
		uint8_t value;
	} mask;

	union {
		struct {
			unsigned int ppu_open_bus : 5;
			unsigned int sprite_overflow : 1;
			unsigned int sprite_0_hit : 1;
			unsigned int vertical_blank_started : 1;
		};
		uint8_t value;
	} status;

	uint8_t oam_address;
	bool address_latch;
} ppu_state_t;

extern ppu_state_t PPU_state;
extern size_t cpu_timer;
extern int scanline;
extern int dot;
void ppu_reset();

#endif