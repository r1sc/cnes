#include "ColorDreams.h"

static uint8_t chr_bank = 0;
static uint8_t prg_bank = 0;

static inline uint16_t ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}

void colordreams_reset() {
	chr_bank = 0;
	prg_bank = 0;
}

uint8_t colordreams_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[ppu_addr_to_ciram_addr(address)];
	}
	return ines.chr_rom[chr_bank * 0x2000 + (address & 0x1FFF)];
}

void colordreams_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[ppu_addr_to_ciram_addr(address)] = value;
	}
}

uint8_t colordreams_cpuRead(uint16_t address) {
	return ines.prg_rom[prg_bank * 0x8000 + (address & 0x7FFF)];
}

void colordreams_cpuWrite(uint16_t address, uint8_t value) {
	if (address >= 0x8000) {
		chr_bank = value >> 4;
		prg_bank = value & 0b11;
	}
}

void colordreams_save_state(void* stream, stream_writer write) {
	write(&chr_bank, sizeof(chr_bank), 1, stream);
	write(&prg_bank, sizeof(prg_bank), 1, stream);
}

void colordreams_load_state(void* stream, stream_reader read) {
	read(&chr_bank, sizeof(chr_bank), 1, stream);
	read(&prg_bank, sizeof(prg_bank), 1, stream);
}