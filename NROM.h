#pragma once
#include <stdint.h>
#include "cartridge.h"
#include "bit.h"

static inline uint16_t nrom_ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}

uint8_t nrom_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[nrom_ppu_addr_to_ciram_addr(address)];
	}
	return ines.chr_rom[address & 0x1FFF];
}

void nrom_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[nrom_ppu_addr_to_ciram_addr(address)] = value;
	}
}

uint8_t nrom_cpuRead(uint16_t address) {
	uint8_t bank_no = (address >> 14) - ines.prg_rom_size_16k_chunks;
	return ines.prg_rom_banks[bank_no][address & 0x3FFF];
}

void nrom_cpuWrite(uint16_t address, uint8_t value) {
	// ignore writes to ROM
}