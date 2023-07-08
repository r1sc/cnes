#pragma once
#include <stdint.h>
#include "cartridge.h"
#include "bit.h"

static inline uint16_t ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}

uint8_t mmc1_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[ppu_addr_to_ciram_addr(address)];
	}
	return ines.chr_rom[address & 0x1FFF];
}

void mmc1_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[ppu_addr_to_ciram_addr(address)] = value;
	}
	if (ines.chr_rom_size_8k_chunks == 0) {
		// CHR RAM
		ines.chr_rom[address & 0x3FFF] = value;
	}
}

static uint8_t selected_bank = 0;
uint8_t mmc1_cpuRead(uint16_t address) {
	//if (address >= 0xC000) {
	//	uint8_t* bank = ines.prg_rom + 0x4000 * (ines.prg_rom_size_16k_chunks - 1);
	//	return bank[address & 0x3FFF];
	//} else {
	//	uint8_t* bank = ines.prg_rom + 0x4000 * selected_bank;
	//	return bank[address & 0x3FFF];
	//}
	
}

static void control_write(uint8_t value) {

}

static uint8_t sr = 0b10000;
void mmc1_cpuWrite(uint16_t address, uint8_t value) {
	if (address >= 0x8000) {
		if (value & 0x80) {
			sr = 0b10000;
		}
		else {
			bool srfull = (sr & 1) == 1;
			sr = (sr >> 1) | ((value & 1) << 4);
			if (srfull) {

				sr = 0b10000;
			}
		}
	}
}