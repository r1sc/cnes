#include "MMC2.h"

static struct {
	uint8_t prg_rom_bank_select;
	uint8_t lower_fd_bank_select;
	uint8_t lower_fe_bank_select;
	uint8_t lower_latch;
	uint8_t upper_fd_bank_select;
	uint8_t upper_fe_bank_select;
	uint8_t upper_latch;
	uint8_t mirroring;
} state;

//static uint8_t ram[1024 * 8]; // 8KB

static inline uint16_t mmc2_ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	if (state.mirroring == 0) {
		ines.ppuaddress_ciram_a10_shift_count = 10;
	} else {
		ines.ppuaddress_ciram_a10_shift_count = 11;
	}
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}


void mmc2_reset() {

}

void mmc2_save_state(void* stream, stream_writer write) {
	write(&state, sizeof(state), 1, stream);
}

void mmc2_load_state(void* stream, stream_reader read) {
	read(&state, sizeof(state), 1, stream);
}

uint8_t mmc2_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[mmc2_ppu_addr_to_ciram_addr(address)];
	}

	uint8_t chr_bank = 0;
	uint8_t value = 0;
	if (address <= 0x0FFF) {
		if (state.lower_latch == 0xFD) {
			chr_bank = state.lower_fd_bank_select;
		} else {
			chr_bank = state.lower_fe_bank_select;
		}
		value = ines.chr_rom[chr_bank * 0x1000 + address];
	} else {
		if (state.upper_latch == 0xFD) {
			chr_bank = state.upper_fd_bank_select;
		} else {
			chr_bank = state.upper_fe_bank_select;
		}
		value = ines.chr_rom[chr_bank * 0x1000 + (address & 0xFFF)];
	}

	if (address == 0xFD8) {
		state.lower_latch = 0xFD;
	} else if (address == 0xFE8) {
		state.lower_latch = 0xFE;
	} else if (address >= 0x1FD8 && address <= 0x1FDF) {
		state.upper_latch = 0xFD;
	} else if (address >= 0x1FE8 && address <= 0x1FEF) {
		state.upper_latch = 0xFE;
	}

	return value;
}

void mmc2_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[mmc2_ppu_addr_to_ciram_addr(address)] = value;
	} else if (ines.is_8k_chr_ram) {
		// CHR RAM
		ines.chr_rom[address & 0x1FFF] = value;
	}
}

uint8_t mmc2_cpuRead(uint16_t address) {
	if (address >= 0x8000 && address <= 0x9FFF) {
		return ines.prg_rom[8192 * state.prg_rom_bank_select + (address & 0x1FFF)];
	}

	size_t addr = (size_t)ines.prg_rom_size_16k_chunks * 0x4000 - (0xFFFF - (int)address) - 1;
	return ines.prg_rom[addr];
}

void mmc2_cpuWrite(uint16_t address, uint8_t value) {
	if (address >= 0xA000 && address <= 0xAFFF) {
		state.prg_rom_bank_select = value & 0b1111;
	} else if (address >= 0xB000 && address <= 0xBFFF) {
		state.lower_fd_bank_select = value & 0b11111;
	} else if (address >= 0xC000 && address <= 0xCFFF) {
		state.lower_fe_bank_select = value & 0b11111;
	} else if (address >= 0xD000 && address <= 0xDFFF) {
		state.upper_fd_bank_select = value & 0b11111;
	} else if (address >= 0xE000 && address <= 0xEFFF) {
		state.upper_fe_bank_select = value & 0b11111;
	} else if (address >= 0xF000 && address <= 0xFFFF) {
		state.mirroring = value & 1;
	}
}
