#pragma once

#include <stdint.h>
#include "bit.h"
#include "nes001.h"

static inline uint16_t unrom_ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}

uint8_t selected_bank = 0;

void unrom_reset() {
	selected_bank = 0;
}

uint8_t unrom_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[unrom_ppu_addr_to_ciram_addr(address)];
	}
	return ines.chr_rom[address & 0x1FFF];
}

void unrom_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[unrom_ppu_addr_to_ciram_addr(address)] = value;
	} else if (ines.chr_rom_size_8k_chunks == 0) {
		// CHR RAM
		ines.chr_rom[address & 0x1FFF] = value;
	}
}

uint8_t unrom_cpuRead(uint16_t address) {
	if (address >= 0xC000) {
		return ines.prg_rom_banks[ines.prg_rom_size_16k_chunks - 1][address & 0x3FFF];
	} else {
		return ines.prg_rom_banks[selected_bank][address & 0x3FFF];
	}
}

void unrom_cpuWrite(uint16_t address, uint8_t value) {
	if (address >= 0x8000) {
		selected_bank = value & 0x0F;
	}
}