#include <stdbool.h>
#include "ppu.h"
#include "cartridge.h"
#include "fake6502.h"

static struct OAMEntry_t {
	uint8_t y;
	uint8_t tile_index;
	uint8_t attributes;
	uint8_t x;
} OAM[64];

static uint8_t* oamptr = (uint8_t*)OAM;

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
} PPUCTRL = { 0 };

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
} PPUMASK = { 0 };

static uint8_t OAMADDR;

static bool PPUADDR_LATCH = false;

union {
	struct {
		unsigned int ppu_open_bus : 5;
		unsigned int sprite_overflow : 1;
		unsigned int sprite_0_hit : 1;
		unsigned int vertical_blank_started : 1;
	};
	uint8_t value;
} PPUSTATUS = { 0 };

static uint8_t palette[32];


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

typedef union {
	struct {
		unsigned int fine_y_offset : 3;
		unsigned int bit_plane : 1;
		unsigned int tile_lo : 4;
		unsigned int tile_hi : 4;
		unsigned int pattern_table_half : 1;
	};
	uint16_t value;
} NAMETABLE_Address_t;

VRAM_Address_t T, V;
static uint8_t fine_x_scroll;


void ppu_internal_bus_write(uint16_t address, uint8_t value) {
	if (address >= 0x3F00) {
		// Palette control
		uint8_t index = address & 0xF;
		palette[index == 0 ? 0 : (address & 0x1F)] = value;
	} else {
		cartridge_ppuWrite(address, value);
	}
}

inline uint8_t ppu_internal_bus_read(uint16_t address) {
	if (address >= 0x3F00) {
		// Palette control
		uint8_t index = address & 0x3;
		return palette[index == 0 ? 0 : (address & 0x1F)];
	}
	return cartridge_ppuRead(address);
}

uint8_t ppudata_buffer = 0;
uint8_t cpu_ppu_bus_read(uint8_t address) {
	uint8_t value = 0;

	switch (address) {
		case 2:
			value = PPUSTATUS.value;
			PPUSTATUS.vertical_blank_started = 0;
			PPUADDR_LATCH = false;
			break;
		case 4:
			value = oamptr[OAMADDR];
			break;
		case 7:
			value = ppudata_buffer;
			ppudata_buffer = ppu_internal_bus_read(V.value);

			if (V.value >= 0x3f00) value = ppudata_buffer; // Do not delay palette reads

			V.value += (PPUCTRL.vram_address_increment ? 32 : 1);
			break;
	}

	return value;
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
			oamptr[OAMADDR++] = value;
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
uint8_t next_pattern_lsb, next_pattern_msb;
uint16_t pattern_plane_0, pattern_plane_1;
uint8_t sprite_lsb[8], sprite_msb[8], num_sprites_on_row;
static struct OAMEntry_t temp_oam[8];

uint8_t next_attribute;
uint16_t attrib_0, attrib_1;

NAMETABLE_Address_t nametable_address = { 0 };
int scanline = 0;
size_t dot = 0;

inline void nametable_fetch() {
	next_tile = ppu_internal_bus_read(0x2000 | (V.value & 0x0FFF));
}

inline void attribute_fetch() {
	next_attribute = ppu_internal_bus_read(0x23C0 | (V.value & 0x0C00) | ((V.value >> 4) & 0x38) | ((V.value >> 2) & 0x07));
	if (V.coarse_y_scroll & 2) next_attribute >>= 4;
	if (V.coarse_x_scroll & 2) next_attribute >>= 2;
	next_attribute &= 0b11;
}

inline void bg_lsb_fetch() {
	nametable_address.fine_y_offset = V.fine_y_scroll;
	nametable_address.bit_plane = 0;
	nametable_address.tile_lo = next_tile & 0xF;
	nametable_address.tile_hi = (next_tile >> 4) & 0xF;
	nametable_address.pattern_table_half = PPUCTRL.background_pattern_table_address;
	next_pattern_lsb = ppu_internal_bus_read(nametable_address.value);
}

inline void bg_msb_fetch() {
	nametable_address.bit_plane = 1;
	next_pattern_msb = ppu_internal_bus_read(nametable_address.value);
}

inline void inc_horiz() {
	if (!PPUMASK.show_background)
		return;

	if (V.coarse_x_scroll == 31) {
		V.coarse_x_scroll = 0;
		V.horizontal_nametable = ~V.horizontal_nametable;
	} else {
		V.coarse_x_scroll++;
	}
}

inline void inc_vert() {
	if (!PPUMASK.show_background)
		return;

	if (V.fine_y_scroll < 7) {
		V.fine_y_scroll++;
	} else {
		V.fine_y_scroll = 0;
		if (V.coarse_y_scroll == 29) {
			V.coarse_y_scroll = 0;
			V.vertical_nametable = ~V.vertical_nametable;
		} else if (V.coarse_y_scroll == 31) {
			V.coarse_y_scroll = 0;
		} else {
			V.coarse_y_scroll++;
		}
	}
}

inline void load_shifters() {
	pattern_plane_0 |= next_pattern_lsb;
	pattern_plane_1 |= next_pattern_msb;

	attrib_0 |= ((next_attribute & 1) ? 0xFF : 0);
	attrib_1 |= ((next_attribute & 2) ? 0xFF : 0);
}

void tick() {

	if (scanline <= 239) {
		if (scanline == -1 && dot == 1) {
			PPUSTATUS.vertical_blank_started = 0;
			PPUSTATUS.sprite_overflow = 0;
			PPUSTATUS.sprite_0_hit = 0;
		}

		if ((dot >= 2 && dot < 258) || (dot >= 321 && dot < 338)) {
			if (PPUMASK.show_background) {
				pattern_plane_0 <<= 1;
				pattern_plane_1 <<= 1;
				attrib_0 <<= 1;
				attrib_1 <<= 1;
			}

			switch ((dot - 1) % 8) {
				case 0:
					load_shifters();
					nametable_fetch();
					break;
				case 2:
					attribute_fetch();
					break;
				case 4:
					bg_lsb_fetch();
					break;
				case 6:
					bg_msb_fetch();
					break;
				case 7:
					inc_horiz();
					break;
			}
		}


		if (dot == 256) {
			inc_vert();
		}

		if (dot == 257) {
			load_shifters();

			if (PPUMASK.show_background || PPUMASK.show_sprites) {
				V.horizontal_nametable = T.horizontal_nametable;
				V.coarse_x_scroll = T.coarse_x_scroll;
			}

			if (PPUMASK.show_sprites) {
				for (size_t i = 0; i < 8; i++) {
					temp_oam[i].x = 0xFF;
					temp_oam[i].y = 0xFF;
					temp_oam[i].attributes = 0xFF;
					temp_oam[i].tile_index = 0xFF;
				}

				num_sprites_on_row = 0;

				if (PPUMASK.show_sprites) {
					for (size_t i = 0; i < 64; i++) {
						int delta_y = scanline - (int)OAM[i].y;
						if (delta_y >= 0 && delta_y < 8) {
							if (num_sprites_on_row < 8) {
								temp_oam[num_sprites_on_row].y = OAM[i].y;
								temp_oam[num_sprites_on_row].x = OAM[i].x;
								temp_oam[num_sprites_on_row].tile_index = OAM[i].tile_index;
								temp_oam[num_sprites_on_row].attributes = OAM[i].attributes;
								num_sprites_on_row++;
								if (num_sprites_on_row == 8) {
									PPUSTATUS.sprite_overflow = 1;
								}
							}
						}
					}
				}
			}
		}

		if (dot == 338 || dot == 340) {
			nametable_fetch();
		}

		if (dot == 340) {
			if (PPUMASK.show_sprites) {
				for (size_t i = 0; i < num_sprites_on_row; i++) {
					nametable_address.fine_y_offset = (uint8_t)(scanline - (int)temp_oam[i].y);
					bool flipped_y = (temp_oam[i].attributes & 0x80) != 0;
					if (flipped_y) {
						nametable_address.fine_y_offset = (uint8_t)(7-nametable_address.fine_y_offset);
					}
					
					nametable_address.bit_plane = 0;
					nametable_address.tile_lo = temp_oam[i].tile_index & 0xF;
					nametable_address.tile_hi = (temp_oam[i].tile_index >> 4) & 0xF;
					nametable_address.pattern_table_half = PPUCTRL.sprite_pattern_table_address;
					sprite_lsb[i] = ppu_internal_bus_read(nametable_address.value);

					nametable_address.bit_plane = 1;
					sprite_msb[i] = ppu_internal_bus_read(nametable_address.value);
				}
			}
		}

		if (PPUMASK.show_background && scanline == -1 && dot >= 280 && dot <= 304) {
			V.coarse_y_scroll = T.coarse_y_scroll;
			V.fine_y_scroll = T.fine_y_scroll;
			V.vertical_nametable = T.vertical_nametable;
		}

		if (scanline >= 0 && dot >= 1 && dot <= 256) {
			uint8_t bg_pixel = 0;
			uint8_t bg_palette = 0;

			if (PPUMASK.show_background) {
				uint16_t bit = 0x8000 >> fine_x_scroll;

				uint8_t lo_bit = (pattern_plane_0 & bit) ? 1 : 0;
				uint8_t hi_bit = (pattern_plane_1 & bit) ? 1 : 0;
				bg_pixel = (hi_bit << 1) | lo_bit;

				uint8_t attr_lo = (attrib_0 & bit) ? 1 : 0;
				uint8_t attr_hi = (attrib_1 & bit) ? 1 : 0;
				bg_palette = (attr_hi << 3) | (attr_lo << 2);
			}

			int first_found = -1;
			uint8_t sprite_pixel = 0;
			uint8_t sprite_palette = 0;

			if (PPUMASK.show_sprites) {
				for (size_t sprite_n = 0; sprite_n < num_sprites_on_row; sprite_n++) {
					if (temp_oam[sprite_n].x == 0) {
						bool flipped_x = (temp_oam[sprite_n].attributes & 0x40) != 0;
						if (sprite_pixel == 0) {
							uint8_t lo_bit = (sprite_lsb[sprite_n] & (flipped_x ? 1 : 0x80)) ? 1 : 0;
							uint8_t hi_bit = (sprite_msb[sprite_n] & (flipped_x ? 1 : 0x80)) ? 1 : 0;
							uint8_t pix = (hi_bit << 1) | lo_bit;
							
							if (pix != 0) {
								first_found = sprite_n;
								if (sprite_n == 0 && bg_pixel != 0 && temp_oam[0].y == OAM[0].y) {
									PPUSTATUS.sprite_0_hit = 1;
								}

								sprite_pixel = pix;
								sprite_palette = (temp_oam[sprite_n].attributes & 0b11) << 2;
							}
						}

						if (flipped_x) {
							sprite_lsb[sprite_n] >>= 1;
							sprite_msb[sprite_n] >>= 1;
						} else {
							sprite_lsb[sprite_n] <<= 1;
							sprite_msb[sprite_n] <<= 1;
						}
					} else {
						temp_oam[sprite_n].x--;
					}
				}
			}

			uint16_t output_palette_location = 0x3F00;
			uint8_t output_pixel = bg_pixel;
			uint8_t output_palette = bg_palette;

			if (PPUMASK.show_sprites) {
				if (bg_pixel == 0 && sprite_pixel != 0) {
					output_pixel = sprite_pixel;
					output_palette = sprite_palette;
					output_palette_location = 0x3F10;
				} else if (sprite_pixel != 0 && bg_pixel != 0) {
					if (((temp_oam[first_found].attributes >> 5) & 1) == 0) {
						output_pixel = sprite_pixel;
						output_palette = sprite_palette;
						output_palette_location = 0x3F10;
					}
				}
			}

			uint8_t palette_index = ppu_internal_bus_read((uint16_t)(output_palette_location | output_palette | output_pixel));
			pixformat_t* pixel = &framebuffer[(size_t)256 * scanline + (dot - 1)];
			pixel->r = palette_colors[palette_index * 3 + 0];
			pixel->g = palette_colors[palette_index * 3 + 1];
			pixel->b = palette_colors[palette_index * 3 + 2];

		}

	}

	if (scanline == 241 && dot == 1) {
		PPUSTATUS.vertical_blank_started = 1;
		if (PPUCTRL.gen_nmi_vblank) {
			nmi6502();
		}
	}
}

size_t cpu_timer = 0;

void tick_frame() {
	scanline = -1;
	while (scanline <= 260) {
		dot = 0;
		while (dot <= 340) {
			if (cpu_timer == 0) {
				step6502();
				cpu_timer = clockticks6502 * 3;
			} else {
				cpu_timer--;
			}

			tick();
			dot++;
		}
		scanline++;
	}
}