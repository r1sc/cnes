#pragma once

#include <stdint.h>
#include <assert.h>
#include "bit.h"
#include "nes001.h"

//Control regs
static uint8_t control_reg = 0;
static uint8_t sr = 0;
static uint8_t shift_count = 0;

static uint8_t chr_bank_4_lo = 0;
static uint8_t chr_bank_4_hi = 0;
static uint8_t char_bank_8 = 0;

static uint8_t prg_bank_lo, prg_bank_hi, prg_bank_32;

static uint8_t mirroring = 0;

static uint8_t ram[8192];


void mmc1_reset() {
	control_reg = 0x1c;
	sr = 0;
	shift_count = 0;

	mirroring = 3;
	chr_bank_4_lo = 0;
	chr_bank_4_hi = 0;
	prg_bank_lo = 0;
	prg_bank_hi = ines.prg_rom_size_16k_chunks - 1;
	prg_bank_32 = 0;
}

static inline uint16_t ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	switch (mirroring) {
		case 0:
			// one-screen, lower bank
			break;
		case 1:
			// one-screen, upper bank
			break;
		case 2:
			// vertical
			ines.ppuaddress_ciram_a10_shift_count = 10;
			break;
		case 3:
			// horizontal
			ines.ppuaddress_ciram_a10_shift_count = 11;
			break;
	}
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}

uint8_t mmc1_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[ppu_addr_to_ciram_addr(address)];
	}
	if (address < 0x2000) {
		if (ines.is_8k_chr_ram) {
			return ines.chr_rom[address];
		} else {
			if (control_reg & 0b10000) {
				// switch two separate 4 KB banks
				if (address < 0x1000) {
					return ines.chr_rom[chr_bank_4_lo * 0x1000 + (address & 0x0FFF)];

				} else {
					return ines.chr_rom[chr_bank_4_hi * 0x1000 + (address & 0x0FFF)];
				}
			} else {
				// switch 8 KB at a time
				return ines.chr_rom[char_bank_8 * 0x2000 + (address & 0x1FFF)];
			}

		}
	}

	assert(false);

	return 0;
}

void mmc1_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[ppu_addr_to_ciram_addr(address)] = value;
	}
	else if (address < 0x2000 && ines.is_8k_chr_ram) {
		ines.chr_rom[address] = value;
	}
}

uint8_t mmc1_cpuRead(uint16_t address) {
	if (address >= 0x6000 && address <= 0x7FFF) {
		return ram[address & 0x1FFF];
	}

	if (address >= 0x8000) {
		if (control_reg & 0b01000) {
			if (address >= 0xC000) {
				return ines.prg_rom[(prg_bank_hi % ines.prg_rom_size_16k_chunks) * 0x4000 + (address & 0x3fff)];
			} else {
				return ines.prg_rom[(prg_bank_lo % ines.prg_rom_size_16k_chunks) * 0x4000 + (address & 0x3fff)];
			}
		} else {
			return ines.prg_rom[prg_bank_32 * 0x8000 + (address & 0x7FFF)];
		}
	}
	return 0;
}

void mmc1_cpuWrite(uint16_t address, uint8_t value) {
	if (address >= 0x6000 && address <= 0x7FFF) {
		ram[address & 0x1FFF] = value;
	} else if (address >= 0x8000) {
		if (value & 0x80) {
			sr = 0;
			shift_count = 0;
		} else {
			sr = (sr >> 1) | ((value & 1) << 4);
			shift_count++;
			if (shift_count == 5) {
				uint8_t reg = (address >> 13) & 0b11;
				switch (reg) {
					case 0:
						// Control
						mirroring = sr & 0b11;
						control_reg = sr & 0x1f;
						break;
					case 1:
						// CHR bank 0						
						if (control_reg & 0b10000) {
							chr_bank_4_lo = sr & 0x1f;
						} else {
							char_bank_8 = sr & 0x1e;
						}
						break;
					case 2:
						if (control_reg & 0b10000) {
							chr_bank_4_hi = sr & 0x1F;
						}
						break;
					case 3:
					{
						uint8_t prg_mode = (control_reg >> 2) & 0x03;
						switch (prg_mode) {
							case 0:
							case 1:
								// switch 32 KB at $8000
								prg_bank_32 = (sr & 0x0e) >> 1;
								break;
							case 2:
								// fix first bank at $8000 and switch 16 KB bank at $C000
								prg_bank_lo = 0;
								prg_bank_hi = sr & 0x0f;
								break;
							case 3:
								// fix last bank at $C000 and switch 16 KB bank at $8000)
								prg_bank_lo = sr & 0x0f;
								prg_bank_hi = ines.prg_rom_size_16k_chunks - 1;
								break;
						}
					}
					break;
				}
				sr = 0;
				shift_count = 0;
			}
		}
	}
}
