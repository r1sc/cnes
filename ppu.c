#include <stdbool.h>
#include "ppu.h"
#include "cartridge.h"
#include "fake6502.h"

static uint8_t OAM[256];

static struct {
	union {
		struct {
			uint8_t nametable_address : 2;
			uint8_t vram_address_increment : 1;
			uint8_t sprite_pattern_table_address : 1;

			uint8_t background_pattern_table_address : 1;
			uint8_t sprite_size : 1;
			uint8_t ppu_master_slave : 1;
			uint8_t gen_nmi_vblank : 1;
		};
		uint8_t value;
	};
} PPUCTRL = { 0 };

static struct {
	union {
		struct {
			uint8_t greyscale : 1;
			uint8_t show_background_left : 1;
			uint8_t show_sprites_left : 1;
			uint8_t show_background : 1;
			uint8_t show_sprites : 1;

			uint8_t emphasize_red : 1;
			uint8_t emphasize_green : 1;
			uint8_t emphasize_blue : 1;
		};
		uint8_t value;
	};
} PPUMASK;

static uint8_t OAMADDR;

static bool PPUADDR_LATCH = false;

static struct {
	union {
		struct {
			uint8_t ppu_open_bus : 5;
			uint8_t sprite_overflow : 1;
			uint8_t sprite_0_hit : 1;
			uint8_t vertical_blank_started : 1;
		};
		uint8_t value;
	};
} PPUSTATUS;

static uint8_t palette[32];

typedef struct {
	union {
		struct {
			uint8_t coarse_x_scroll : 5;
			uint8_t coarse_y_scroll : 5;
			union {
				struct {
					uint8_t horizontal_nametable : 1;
					uint8_t vertical_nametable : 1;
				};
				uint8_t nametable_select;
			};
			uint8_t fine_y_scroll : 3;
		};
		uint16_t value : 15;
	};
} VRAM_Address_t;

static VRAM_Address_t T, V;
static uint8_t fine_x_scroll;


void ppu_internal_bus_write(uint16_t address, uint8_t value) {
	if (address & 0x3F00) {
		// Palette control
		uint8_t index = address & 3;
		palette[index == 0 ? 0 : (address & 0x1F)] = value;
	} else {
		cartridge_ppuWrite(address, value);
	}
}

uint8_t ppu_internal_bus_read(uint16_t address) {
	if (address & 0x3F00) {
		// Palette control
		uint8_t index = address & 3;
		return palette[index == 0 ? 0 : (address & 0x1F)];
	}
	return cartridge_ppuRead(address);
}

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
			uint8_t value = ppu_internal_bus_read(V.value);
			uint8_t vram_address_increment = PPUCTRL.vram_address_increment ? 32 : 1;
			V.value += vram_address_increment;
			return value;
		}
	}

	return 0;
}

void cpu_ppu_bus_write(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			PPUCTRL.value = value;
			T.nametable_select = value & 0b11;
			break;
		case 1:
			PPUMASK.value = value;
			break;
		case 3:
			OAMADDR = value;
			break;
		case 4:
			OAM[OAMADDR] = value;
			OAMADDR++;
			break;
		case 5:
			if (PPUADDR_LATCH) {
				T.coarse_y_scroll = value >> 3;
				T.fine_y_scroll = value & 0b111;
			} else {
				T.coarse_x_scroll = value >> 3;
				fine_x_scroll = value & 0b111;
			}
			PPUADDR_LATCH = !PPUADDR_LATCH;
			break;
		case 6:
			if (PPUADDR_LATCH) {
				T.value = T.value | value;
				V.value = T.value;
			} else {
				T.value = T.value | (value << 8);
				T.value &= 0x7FFF; // Clear top bit
			}
			PPUADDR_LATCH = !PPUADDR_LATCH;
			break;
		case 7:
			ppu_internal_bus_write(V.value, value);
			uint8_t vram_address_increment = PPUCTRL.vram_address_increment ? 32 : 1;
			V.value += vram_address_increment;
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

uint8_t bg_shift;
size_t scanline;
size_t dot;

/// <summary>
/// Advances the PPU one full frame
/// Also drives the CPU every 3rd PPU cycle
/// </summary>
void tick_frame() {
	size_t cpu_timer = 0;
	size_t tile_fetch_counter = 8;

	while(scanline < 262) {
		while (dot < 341) {
			if (hold_cpu) return;

			// Step CPU
			if (cpu_timer == 0) {
				step6502();
				cpu_timer = clockticks6502 - 3;
			} else {
				cpu_timer--;
			}

			uint8_t bg_bit = bg_shift >> fine_x_scroll;
			bg_shift >>= 1;

			if (tile_fetch_counter == 0) {
				bg_shift = ppu_internal_bus_read(0x2000 | (V.value & 0xFFF));
				tile_fetch_counter = 8;
			} else {
				tile_fetch_counter--;
			}

			if (PPUMASK.show_background || PPUMASK.show_sprites) {
				// Rendering enabled

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

				} else if (dot == 257) {
					V.coarse_x_scroll = T.coarse_x_scroll;
					V.horizontal_nametable = T.horizontal_nametable;
				}

				if (V.coarse_x_scroll == 31) {
					V.coarse_x_scroll = 0;
					V.horizontal_nametable = !V.horizontal_nametable;
				} else {
					V.coarse_x_scroll++;
				}

				if (scanline >= 0 && scanline <= 239) {
					// Visible scanline

				} else if (scanline == 261) {
					// Pre-render scanlne

					if (dot == 1) {
						PPUSTATUS.vertical_blank_started = 0;
						PPUSTATUS.sprite_overflow = 0;
					}

					if (dot >= 280 && dot <= 304) {
						V.coarse_y_scroll = T.coarse_y_scroll;
						V.vertical_nametable = T.vertical_nametable;
						V.fine_y_scroll = T.fine_y_scroll;
					}
				}
			}
			dot++;
		}
		dot = 0;
		scanline++;
	}
	scanline = 0;
}