#include <stdbool.h>
#include "ppu.h"

#include "nes001.h"
#include "apu.h"
#include "fake6502.h"
#include "include/cnes.h"

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

static const uint8_t palette_colors[192] =
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

ppu_state_t PPU_state = { 0 };

void ppu_reset() {
	for (size_t i = 0; i < 32; i++) {
		PPU_state.palette[i] = 0;
	}

	PPU_state.oam_address = 0;
	PPU_state.address_latch = false;
	PPU_state.status.value = 0;
	PPU_state.mask.value = 0;
	PPU_state.control.value = 0;
	PPU_state.fine_x_scroll = 0;
	PPU_state.V.value = 0;
	PPU_state.T.value = 0;
	PPU_state.ppudata_buffer = 0;
}

// Frame "local" data
static uint8_t next_tile;
static uint8_t next_pattern_lsb, next_pattern_msb;
static uint16_t pattern_plane_0, pattern_plane_1;
static uint8_t sprite_lsb[8], sprite_msb[8], num_sprites_on_row;
static struct OAMEntry_t temp_oam[8];
static uint8_t next_attribute;
static uint16_t attrib_0, attrib_1;
static NAMETABLE_Address_t nametable_address = { 0 };

size_t cpu_timer = 0;
static size_t apu_timer = 0;

// Public data
int scanline = 0;
int dot = 0;
pixformat_t framebuffer[256 * 240];

static inline void ppu_internal_bus_write(uint16_t address, uint8_t value) {
	if (address >= 0x3F00 && address <= 0x3FFF) {
		// Palette control
		uint8_t index = address & 0xF;
		PPU_state.palette[index == 0 ? 0 : (address & 0x1F)] = value;
	} else {
		cartridge_ppuWrite(address, value);
	}
}

static inline uint8_t ppu_internal_bus_read(uint16_t address) {
	if (address >= 0x3F00 && address <= 0x3FFF) {
		// Palette control
		uint8_t index = address & 0x3;
		return PPU_state.palette[index == 0 ? 0 : (address & 0x1F)];
	} else {
		return cartridge_ppuRead(address);
	}
}


uint8_t cpu_ppu_bus_read(uint8_t address) {
	uint8_t value = 0;

	switch (address) {
		case 2:
			value = PPU_state.status.value;
			PPU_state.status.vertical_blank_started = 0;
			PPU_state.address_latch = false;
			break;
		case 4:
			value = ((uint8_t*)PPU_state.OAM)[PPU_state.oam_address];
			break;
		case 7:
			value = PPU_state.ppudata_buffer;
			PPU_state.ppudata_buffer = ppu_internal_bus_read(PPU_state.V.value);

			if (PPU_state.V.value >= 0x3f00 && PPU_state.V.value <= 0x3fff) {
				value = PPU_state.ppudata_buffer; // Do not delay palette reads
			}

			PPU_state.V.value += (PPU_state.control.vram_address_increment ? 32 : 1);
			break;
	}

	return value;
}

void cpu_ppu_bus_write(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			PPU_state.control.value = value;
			PPU_state.T.horizontal_nametable = value & 1;
			PPU_state.T.vertical_nametable = (value >> 1) & 1;
			break;
		case 1:
			PPU_state.mask.value = value;
			break;
		case 3:
			PPU_state.oam_address = value;
			break;
		case 4:
			((uint8_t*)PPU_state.OAM)[PPU_state.oam_address++] = value;
			break;
		case 5:
			if (PPU_state.address_latch) {
				PPU_state.T.coarse_y_scroll = (value >> 3) & 0b11111;
				PPU_state.T.fine_y_scroll = value & 0b111;
			} else {
				PPU_state.T.coarse_x_scroll = (value >> 3) & 0b11111;
				PPU_state.fine_x_scroll = value & 0b111;
			}
			PPU_state.address_latch = !PPU_state.address_latch;
			break;
		case 6:
			if (PPU_state.address_latch) {
				PPU_state.T.value = (PPU_state.T.value & 0xFF00) | value;
				PPU_state.V.value = PPU_state.T.value;
			} else {
				PPU_state.T.value = ((((uint16_t)value & 0x7F) << 8) | (PPU_state.T.value & 0xFF)) & 0x7FFF;
			}
			PPU_state.address_latch = !PPU_state.address_latch;
			break;
		case 7:
			ppu_internal_bus_write(PPU_state.V.value, value);
			PPU_state.V.value += (PPU_state.control.vram_address_increment ? 32 : 1);
			break;
	}
}


static inline void nametable_fetch() {
	next_tile = cartridge_ppuRead(0x2000 | (PPU_state.V.value & 0x0FFF));
}

static inline void attribute_fetch() {
	next_attribute = cartridge_ppuRead(0x23C0 | (PPU_state.V.value & 0x0C00) | ((PPU_state.V.value >> 4) & 0x38) | ((PPU_state.V.value >> 2) & 0x07));
	if (PPU_state.V.coarse_y_scroll & 2) next_attribute >>= 4;
	if (PPU_state.V.coarse_x_scroll & 2) next_attribute >>= 2;
	next_attribute &= 0b11;
}

static inline void bg_lsb_fetch() {
	nametable_address.fine_y_offset = PPU_state.V.fine_y_scroll;
	nametable_address.bit_plane = 0;
	nametable_address.tile_lo = next_tile & 0xF;
	nametable_address.tile_hi = (next_tile >> 4) & 0xF;
	nametable_address.pattern_table_half = PPU_state.control.background_pattern_table_address;
	next_pattern_lsb = cartridge_ppuRead(nametable_address.value);
}

static inline void bg_msb_fetch() {
	nametable_address.bit_plane = 1;
	next_pattern_msb = cartridge_ppuRead(nametable_address.value);
}

static inline void inc_horiz() {
	if (!PPU_state.mask.show_background)
		return;

	if (PPU_state.V.coarse_x_scroll == 31) {
		PPU_state.V.coarse_x_scroll = 0;
		PPU_state.V.horizontal_nametable = ~PPU_state.V.horizontal_nametable;
	} else {
		PPU_state.V.coarse_x_scroll++;
	}
}

static inline void inc_vert() {
	if (!PPU_state.mask.show_background)
		return;

	if (PPU_state.V.fine_y_scroll < 7) {
		PPU_state.V.fine_y_scroll++;
	} else {
		PPU_state.V.fine_y_scroll = 0;
		if (PPU_state.V.coarse_y_scroll == 29) {
			PPU_state.V.coarse_y_scroll = 0;
			PPU_state.V.vertical_nametable = ~PPU_state.V.vertical_nametable;
		} else if (PPU_state.V.coarse_y_scroll == 31) {
			PPU_state.V.coarse_y_scroll = 0;
		} else {
			PPU_state.V.coarse_y_scroll++;
		}
	}
}

static inline void load_shifters() {
	pattern_plane_0 |= next_pattern_lsb;
	pattern_plane_1 |= next_pattern_msb;

	attrib_0 |= ((next_attribute & 1) ? 0xFF : 0);
	attrib_1 |= ((next_attribute & 2) ? 0xFF : 0);
}


void tick_frame() {
	if (!rom_loaded) return;
	for (scanline = -1; scanline <= 260; scanline++) {
		for (dot = 0; dot <= 340; dot++) {
			if (cpu_timer == 0) {
				step6502();
				cpu_timer = clockticks6502 + clockticks6502 + clockticks6502;
			} else {
				cpu_timer--;
			}

			if (apu_timer == 2) {
				apu_tick_triangle();
			}

			if (apu_timer == 5) {
				apu_tick_triangle();
				apu_tick(scanline + 1);
				apu_timer = 0;
			} else {
				apu_timer++;
			}

			if (scanline <= 239) {
				if (scanline == -1 && dot == 1) {
					PPU_state.status.vertical_blank_started = 0;
					PPU_state.status.sprite_overflow = 0;
					PPU_state.status.sprite_0_hit = 0;
				}

				if ((dot >= 2 && dot < 258) || (dot >= 321 && dot < 338)) {
					if (PPU_state.mask.show_background) {
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
				} else if (dot == 257) {
					load_shifters();

					if (PPU_state.mask.show_background || PPU_state.mask.show_sprites) {
						PPU_state.V.horizontal_nametable = PPU_state.T.horizontal_nametable;
						PPU_state.V.coarse_x_scroll = PPU_state.T.coarse_x_scroll;
					}

					if (PPU_state.mask.show_sprites) {
						for (size_t i = 0; i < 8; i++) {
							temp_oam[i].x = 0xFF;
							temp_oam[i].y = 0xFF;
							temp_oam[i].attributes = 0xFF;
							temp_oam[i].tile_index = 0xFF;
						}

						num_sprites_on_row = 0;

						if (PPU_state.mask.show_sprites) {
							for (size_t i = 0; i < 64; i++) {
								int delta_y = scanline - (int)PPU_state.OAM[i].y;
								if (delta_y >= 0 && (PPU_state.control.sprite_size == 0 ? delta_y < 8 : delta_y < 16)) {
									if (num_sprites_on_row < 8) {
										temp_oam[num_sprites_on_row].y = PPU_state.OAM[i].y;
										temp_oam[num_sprites_on_row].tile_index = PPU_state.OAM[i].tile_index;
										temp_oam[num_sprites_on_row].attributes = PPU_state.OAM[i].attributes;
										temp_oam[num_sprites_on_row].x = PPU_state.OAM[i].x;
										num_sprites_on_row++;
										if (num_sprites_on_row == 8) {
											PPU_state.status.sprite_overflow = 1;
										}
									}
								}
							}
						}
					}
				} else if (dot == 338) {
					nametable_fetch();
				} else if (dot == 340) {
					if (cartridge_scanline != NULL && (PPU_state.mask.show_background || PPU_state.mask.show_sprites)) {
						cartridge_scanline();
					}
					nametable_fetch();
					if (PPU_state.mask.show_sprites) {
						for (size_t i = 0; i < num_sprites_on_row; i++) {
							if (PPU_state.control.sprite_size) {
								// Tall sprites

								bool flipped_y = (temp_oam[i].attributes & 0x80) != 0;
								int y_offset = scanline - (int)temp_oam[i].y;

								if (flipped_y) {
									y_offset = 15 - y_offset;
								}

								uint8_t sprite_index = temp_oam[i].tile_index & 0xFE;
								if (y_offset > 7) {
									y_offset -= 8;
									sprite_index++;
								}

								nametable_address.fine_y_offset = (uint8_t)y_offset;

								nametable_address.bit_plane = 0;
								nametable_address.tile_lo = sprite_index;
								nametable_address.tile_hi = (sprite_index >> 4) & 0xF;
								nametable_address.pattern_table_half = temp_oam[i].tile_index & 1;
							} else {
								// Normal sprites
								nametable_address.fine_y_offset = (uint8_t)(scanline - (int)temp_oam[i].y);
								bool flipped_y = (temp_oam[i].attributes & 0x80) != 0;
								if (flipped_y) {
									nametable_address.fine_y_offset = (uint8_t)(7 - nametable_address.fine_y_offset);
								}

								nametable_address.bit_plane = 0;
								nametable_address.tile_lo = temp_oam[i].tile_index & 0xF;
								nametable_address.tile_hi = (temp_oam[i].tile_index >> 4) & 0xF;
								nametable_address.pattern_table_half = PPU_state.control.sprite_pattern_table_address;
							}

							sprite_lsb[i] = cartridge_ppuRead(nametable_address.value);
							nametable_address.bit_plane = 1;
							sprite_msb[i] = cartridge_ppuRead(nametable_address.value);
						}
					}
				}

				if (PPU_state.mask.show_background && scanline == -1 && dot >= 280 && dot <= 304) {
					PPU_state.V.coarse_y_scroll = PPU_state.T.coarse_y_scroll;
					PPU_state.V.fine_y_scroll = PPU_state.T.fine_y_scroll;
					PPU_state.V.vertical_nametable = PPU_state.T.vertical_nametable;
				}

				if (scanline >= 0 && dot >= 1 && dot <= 256) {
					uint8_t bg_pixel = 0;
					uint8_t bg_palette = 0;

					bool show_background = PPU_state.mask.show_background && (PPU_state.mask.show_background_left || dot > 8);

					if (show_background) {
						uint16_t bit = 0x8000 >> PPU_state.fine_x_scroll;

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

					if (PPU_state.mask.show_sprites) {
						for (int sprite_n = 0; sprite_n < num_sprites_on_row; sprite_n++) {
							if (temp_oam[sprite_n].x == 0) {
								bool flipped_x = (temp_oam[sprite_n].attributes & 0x40) != 0;
								if (sprite_pixel == 0) {
									uint8_t lo_bit = (sprite_lsb[sprite_n] & (flipped_x ? 1 : 0x80)) ? 1 : 0;
									uint8_t hi_bit = (sprite_msb[sprite_n] & (flipped_x ? 1 : 0x80)) ? 1 : 0;
									uint8_t pix = (hi_bit << 1) | lo_bit;

									if (pix != 0) {
										first_found = sprite_n;
										if (sprite_n == 0 && bg_pixel != 0 && temp_oam[0].y == PPU_state.OAM[0].y) {
											PPU_state.status.sprite_0_hit = 1;
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

					uint16_t output_palette_location = 0x00;
					uint8_t output_pixel = bg_pixel;
					uint8_t output_palette = bg_palette;

					bool show_sprites = PPU_state.mask.show_sprites && (PPU_state.mask.show_background_left || dot > 8);

					if (show_sprites) {
						if (bg_pixel == 0 && sprite_pixel != 0) {
							output_pixel = sprite_pixel;
							output_palette = sprite_palette;
							output_palette_location = 0x10;
						} else if (sprite_pixel != 0 && bg_pixel != 0) {
							if (((temp_oam[first_found].attributes >> 5) & 1) == 0) {
								output_pixel = sprite_pixel;
								output_palette = sprite_palette;
								output_palette_location = 0x10;
							}
						}
					}

					//uint8_t palette_index = ppu_internal_bus_read((uint16_t)(output_palette_location | output_palette | output_pixel)) & 0x3f;
					uint16_t palette_addr = output_palette_location | output_palette | output_pixel;
					uint8_t palette_index = PPU_state.palette[(palette_addr & 0x3) == 0 ? 0 : (palette_addr & 0x1F)] & 0x3f;
					pixformat_t* pixel = &framebuffer[(size_t)256 * scanline + (dot - 1)];
					pixel->r = palette_colors[palette_index * 3 + 0];
					pixel->g = palette_colors[palette_index * 3 + 1];
					pixel->b = palette_colors[palette_index * 3 + 2];
				}
			} else if (scanline == 241 && dot == 1) {
				PPU_state.status.vertical_blank_started = 1;
				if (PPU_state.control.gen_nmi_vblank) {
					nmi6502();
				}
			}
		}
	}
}
