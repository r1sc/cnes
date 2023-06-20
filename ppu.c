#include <stdbool.h>
#include "ppu.h"
#include "cartridge.h"

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
static struct {
	union {
		struct {
			uint8_t lo;
			uint8_t hi;
		};
		uint16_t value;
	};
}PPUADDR;

static bool PPUADDR_LATCH = false;

static uint8_t PPUSCROLL_X, PPUSCROLL_Y;

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
			uint8_t value = ppu_internal_bus_read(PPUADDR.value);
			uint8_t vram_address_increment = PPUCTRL.vram_address_increment ? 32 : 1;
			PPUADDR.value += vram_address_increment;
			return value;
		}
	}

	return 0;
}

void cpu_ppu_bus_write(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			PPUCTRL.value = value;
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
				PPUSCROLL_Y = value;
			} else {
				PPUSCROLL_X = value;
			}
			PPUADDR_LATCH = !PPUADDR_LATCH;
			break;
		case 6:
			if (PPUADDR_LATCH) {
				PPUADDR.lo = value;
			} else {
				PPUADDR.hi = value;
			}
			PPUADDR_LATCH = !PPUADDR_LATCH;
			break;
		case 7:
			ppu_internal_bus_write(PPUADDR.value, value);
			uint8_t vram_address_increment = PPUCTRL.vram_address_increment ? 32 : 1;
			PPUADDR.value += vram_address_increment;
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

/// <summary>
/// Advances the PPU one full frame
/// Also drives the CPU every 3rd PPU cycle
/// </summary>
void tick_frame() {

}