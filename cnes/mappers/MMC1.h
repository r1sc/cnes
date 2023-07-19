#pragma once

#include <stdint.h>
#include <assert.h>
#include "bit.h"
#include "nes001.h"

//Control regs
static bool chr_rom_mode = false;
static uint8_t prg_rom_mode = 0;
static uint8_t mirroring = 0;

// CHR option
static uint8_t chr_bank_0 = 0;
static uint8_t chr_bank_1 = 0;


// PRG option
static uint8_t prg_bank;
static bool prg_ram_chip_enabled = false;

static uint8_t ram[8192];

static uint8_t sr = 0;
static uint8_t shift_count = 0;

void mmc1_reset() {
	sr = 0;
	shift_count = 0;
	mirroring = 0;
	prg_rom_mode = 3;
	chr_rom_mode = true;
	chr_bank_0 = 0;
	chr_bank_1 = 0;
	prg_bank = 0;
	prg_ram_chip_enabled = false;
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
	switch (chr_rom_mode) {
		case false:
			// switch 8 KB at a time
			return ines.chr_rom[address & 0x1FFF];
			break;
		case true:
			// switch two separate 4 KB banks
			if (address >= 0x1000) {
				return ines.chr_rom[(chr_bank_1 << 12) | (address & 0x0FFF)];
			} else {
				return ines.chr_rom[(chr_bank_0 << 12) | (address & 0x0FFF)];
			}
			break;
	}
	assert(false);
	return 0;
}

void mmc1_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[ppu_addr_to_ciram_addr(address)] = value;
	}
	//if (ines.chr_rom_size_8k_chunks == 0) {
	//	// CHR RAM
	//	ines.chr_rom[address & 0x1FFF] = value;
	//} else {
		switch (chr_rom_mode) {
			case false:
				// switch 8 KB at a time
				ines.chr_rom[address & 0x1FFF] = value;
				break;
			case true:
			{
				int hej = 12;
			}
				break;
		}
	//}
}

uint8_t mmc1_cpuRead(uint16_t address) {
	if (address >= 0x6000 && address <= 0x7FFF) {
		return ram[address & 0x1FFF];
	}

	if (address >= 0x8000) {
		switch (prg_rom_mode) {
			case 0:
			case 1:
				// switch 32 KB at $8000, ignoring low bit of bank number
				return ines.prg_rom_banks[(prg_bank >> 1) && 0b1111][address & 0x7FFF];
				break;
			case 2:
				// fix first bank at $8000 and switch 16 KB bank at $C000
				if (address >= 0xC000) {
					return ines.prg_rom_banks[prg_bank][address & 0x3FFF];
				} else {
					return ines.prg_rom_banks[0][address & 0x3FFF];
				}
				break;
			case 3:
				// fix last bank at $C000 and switch 16 KB bank at $8000)
				if (address >= 0xC000) {
					return ines.prg_rom_banks[ines.prg_rom_size_16k_chunks - 1][address & 0x3FFF];
				} else {
					return ines.prg_rom_banks[prg_bank][address & 0x3FFF];
				}
				break;
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
						prg_rom_mode = (sr >> 2) & 0b11;
						chr_rom_mode = (sr & 0x10) != 0;
						break;
					case 1:
						// CHR bank 0
						chr_bank_0 = sr & 0x1F;
						if (!chr_rom_mode) {
							chr_bank_0 = chr_bank_0 & 0x1E;
						}
						break;
					case 2:
						chr_bank_1 = sr & 0x1F;
						break;
					case 3:
						prg_bank = sr & 0b1111;
						prg_ram_chip_enabled = sr & 0b10000;
						break;
				}
				sr = 0;
				shift_count = 0;
			}
		}
	}
}
