#include <stdbool.h>
#include "ppu.h"
#include "cartridge.h"
#include "fake6502.h"

static uint8_t OAM[256];

static struct {
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
	};
} PPUCTRL = { 0 };

static struct {
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
	};
} PPUMASK = { 0 };

static uint8_t OAMADDR;

static bool PPUADDR_LATCH = false;

static struct {
	union {
		struct {
			unsigned int ppu_open_bus : 5;
			unsigned int sprite_overflow : 1;
			unsigned int sprite_0_hit : 1;
			unsigned int vertical_blank_started : 1;
		};
		uint8_t value;
	};
} PPUSTATUS = { 0 };

static uint8_t palette[32];

typedef struct {
	union {
		struct {
			unsigned int coarse_x_scroll : 5;
			unsigned int coarse_y_scroll : 5;
			unsigned int horizontal_nametable : 1;
			unsigned int vertical_nametable : 1;
			unsigned int fine_y_scroll : 3;
		};
		uint16_t value : 15;
	};
} VRAM_Address_t;

typedef struct {
	union {
		struct {
			unsigned int fine_y_offset : 3;
			unsigned int bit_plane : 1;
			unsigned int tile_lo : 4;
			unsigned int tile_hi : 4;
			unsigned int pattern_table_half : 1;
		};
		uint16_t value;
	};
} NAMETABLE_Address_t;

static VRAM_Address_t T, V;
static uint8_t fine_x_scroll;


void ppu_internal_bus_write(uint16_t address, uint8_t value) {
	if (address >= 0x3F00) {
		// Palette control
		uint8_t index = address & 3;
		palette[index == 0 ? 0 : (address & 0x1F)] = value;
	} else {
		cartridge_ppuWrite(address, value);
	}
}

inline uint8_t ppu_internal_bus_read(uint16_t address) {
	if (address >= 0x3F00) {
		// Palette control
		uint8_t index = address & 3;
		return palette[index == 0 ? 0 : (address & 0x1F)];
	}
	return cartridge_ppuRead(address);
}

uint8_t ppudata_buffer = 0;
uint8_t cpu_ppu_bus_read(uint8_t address) {
	switch (address) {
		case 2:
		{
			uint8_t status = PPUSTATUS.value;
			PPUSTATUS.vertical_blank_started = 0; // Clear vblank (bit 7)
			PPUADDR_LATCH = false;
			return status;
		}
		case 4:
			return OAM[OAMADDR];
		case 7:
		{
			uint8_t value = ppudata_buffer;
			ppudata_buffer = ppu_internal_bus_read(V.value);
			V.value += (PPUCTRL.vram_address_increment ? 32 : 1);
			return value;
		}
	}

	return 0;
}

void cpu_ppu_bus_write(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			PPUCTRL.value = value;
			T.horizontal_nametable = value & 1;
			T.vertical_nametable = (value >> 1) & 1;
			break;
		case 1:
			PPUMASK.value = value;
			break;
		case 3:
			OAMADDR = value;
			break;
		case 4:
			OAM[OAMADDR++] = value;
			break;
		case 5:
			if (PPUADDR_LATCH) {
				T.coarse_y_scroll = (value >> 3) & 0b11111;
				T.fine_y_scroll = value & 0b111;
			} else {
				T.coarse_x_scroll = (value >> 3) & 0b11111;
				fine_x_scroll = value & 0b111;
			}
			PPUADDR_LATCH = !PPUADDR_LATCH;
			break;
		case 6:
			if (PPUADDR_LATCH) {
				T.value = (T.value & 0xFF00) | value;
				V.value = T.value;
			} else {
				uint16_t temp = value & 0x7F;
				temp <<= 8;
				T.value = temp | (T.value & 0xFF);
				T.value = T.value & 0x7FFF; // Clear top bit
			}
			PPUADDR_LATCH = !PPUADDR_LATCH;
			break;
		case 7:
			ppu_internal_bus_write(V.value, value);
			V.value += (PPUCTRL.vram_address_increment ? 32 : 1);
			break;
	}
}

uint8_t palette_colors[192] =
{
	0x52, 0x52, 0x52, 0x01, 0x1A, 0x51, 0x0F, 0x0F, 0x65, 0x23, 0x06, 0x63, 0x36, 0x03, 0x4B, 0x40,
	0x04, 0x26, 0x3F, 0x09, 0x04, 0x32, 0x13, 0x00, 0x1F, 0x20, 0x00, 0x0B, 0x2A, 0x00, 0x00, 0x2F,
	0x00, 0x00, 0x2E, 0x0A, 0x00, 0x26, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xA0, 0xA0, 0xA0, 0x1E, 0x4A, 0x9D, 0x38, 0x37, 0xBC, 0x58, 0x28, 0xB8, 0x75, 0x21, 0x94, 0x84,
	0x23, 0x5C, 0x82, 0x2E, 0x24, 0x6F, 0x3F, 0x00, 0x51, 0x52, 0x00, 0x31, 0x63, 0x00, 0x1A, 0x6B,
	0x05, 0x0E, 0x69, 0x2E, 0x10, 0x5C, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0x69, 0x9E, 0xFC, 0x89, 0x87, 0xFF, 0xAE, 0x76, 0xFF, 0xCE, 0x6D, 0xF1, 0xE0,
	0x70, 0xB2, 0xDE, 0x7C, 0x70, 0xC8, 0x91, 0x3E, 0xA6, 0xA7, 0x25, 0x81, 0xBA, 0x28, 0x63, 0xC4,
	0x46, 0x54, 0xC1, 0x7D, 0x56, 0xB3, 0xC0, 0x3C, 0x3C, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0xBE, 0xD6, 0xFD, 0xCC, 0xCC, 0xFF, 0xDD, 0xC4, 0xFF, 0xEA, 0xC0, 0xF9, 0xF2,
	0xC1, 0xDF, 0xF1, 0xC7, 0xC2, 0xE8, 0xD0, 0xAA, 0xD9, 0xDA, 0x9D, 0xC9, 0xE2, 0x9E, 0xBC, 0xE6,
	0xAE, 0xB4, 0xE5, 0xC7, 0xB5, 0xDF, 0xE4, 0xA9, 0xA9, 0xA9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

uint8_t next_tile;
uint16_t pattern_plane_0_sr;
uint16_t pattern_plane_1_sr;
uint8_t palette_attribute;
uint8_t next_palette_attribute;
NAMETABLE_Address_t nametable_address = { 0 };

size_t scanline = 0;
size_t dot = 0;
size_t tile_fetch_counter = 0;
size_t cpu_timer = 0;

/// <summary>
/// Advances the PPU one full frame
/// Also drives the CPU every 3rd PPU cycle
/// </summary>
void tick_frame() {
	// The PPU has 262 scanlines and 341 clocks for each scanline
	while (scanline <= 261) {
		tile_fetch_counter = 0;
		while (dot <= 340) {
			if (hold_clock) return;

			// Step CPU
			if (cpu_timer == 0) {
				step6502();
				cpu_timer = clockticks6502 * 3 - 1;
			} else {
				cpu_timer--;
			}

			bool rendering_enabled = PPUMASK.show_background || PPUMASK.show_sprites;
			bool visible_pixel = rendering_enabled && scanline < 240 && dot < 256;
			bool fetch_data = rendering_enabled && (scanline < 240 || scanline == 261);

			if (visible_pixel) {
				uint8_t bg_bit_0 = ((pattern_plane_0_sr >> fine_x_scroll) >> 7) & 1;
				uint8_t bg_bit_1 = ((pattern_plane_1_sr >> fine_x_scroll) >> 7) & 1;
				bool shift_one = ((V.coarse_x_scroll >> 4) & 1) == 1;

				uint8_t attr = palette_attribute;
				if ((V.coarse_y_scroll & 2) == 2) attr = attr >> 4;
				if ((V.coarse_x_scroll & 2) == 2) attr = attr >> 2;
				attr = (attr & 0b11);

				uint8_t palette_index = ppu_internal_bus_read((uint16_t)(0x3F00 | (attr << 2) | (bg_bit_1 << 1) | bg_bit_0));
				pixformat_t* pixel = &framebuffer[scanline * 256 + dot];
				pixel->r = palette_colors[palette_index * 3];
				pixel->g = palette_colors[palette_index * 3 + 1];
				pixel->b = palette_colors[palette_index * 3 + 2];

				pattern_plane_0_sr = pattern_plane_0_sr << 1;
				pattern_plane_1_sr = pattern_plane_1_sr << 1;
			}

			if (fetch_data) {
				if (dot == 256) {
					// Increment vertical position in v
					if (V.fine_y_scroll < 7) {
						V.fine_y_scroll++;
					} else {
						V.fine_y_scroll = 0;
						if (V.coarse_y_scroll == 29) {
							V.coarse_y_scroll = 0;
							V.vertical_nametable = !V.vertical_nametable;
						} else if (V.coarse_y_scroll == 31) {
							V.coarse_y_scroll = 0;
						} else {
							V.coarse_y_scroll++;
						}
					}
				}

				if (dot <= 256 || dot >= 321) {

					// Nametable && attribute fetch
					if (tile_fetch_counter == 7) {
						next_tile = ppu_internal_bus_read(0x2000 | (V.value & 0xFFF));
						next_palette_attribute = ppu_internal_bus_read(0x23C0 | (V.value & 0x0C00) | ((V.value >> 4) & 0x38) | ((V.value >> 2) & 0x07));

						nametable_address.fine_y_offset = V.fine_y_scroll;
						nametable_address.bit_plane = 0;
						nametable_address.tile_lo = next_tile & 0xF;
						nametable_address.tile_hi = (next_tile >> 4) & 0xF;
						nametable_address.pattern_table_half = PPUCTRL.background_pattern_table_address;
						pattern_plane_0_sr |= ppu_internal_bus_read(nametable_address.value);

						nametable_address.bit_plane = 1;
						pattern_plane_1_sr |= ppu_internal_bus_read(nametable_address.value);

						// Inc(horiz)
						if (V.coarse_x_scroll == 31) { // if coarse X == 31
							V.coarse_x_scroll = 0;
							V.horizontal_nametable = !V.horizontal_nametable; // switch horizontal nametable
						} else {
							V.coarse_x_scroll += 1;                // increment coarse X
						}
						tile_fetch_counter = 0;

						palette_attribute = next_palette_attribute;
					} else {
						tile_fetch_counter++;
					}

				} else if (dot == 257) {
					V.horizontal_nametable = T.horizontal_nametable;
					V.coarse_x_scroll = T.coarse_x_scroll;
				}

				if (scanline == 261) {
					if (dot == 1) {
						PPUSTATUS.vertical_blank_started = 0;
					} else if (dot >= 280 && dot <= 304) {
						V.coarse_y_scroll = T.coarse_y_scroll;
						V.fine_y_scroll = T.fine_y_scroll;
						V.vertical_nametable = T.vertical_nametable;
					}
				}
			}

			if (scanline == 241 && dot == 1) {
				PPUSTATUS.vertical_blank_started = 1;
				if (PPUCTRL.gen_nmi_vblank) {
					nmi6502();
				}
			}
			dot++;
		}
		dot = 0;
		scanline++;
	}
	scanline = 0;
}