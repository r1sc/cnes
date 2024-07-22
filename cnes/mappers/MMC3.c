#include "MMC3.h"
#include "../nes001.h"
#include "../ppu.h"

static uint8_t ram[1024 * 8]; // 8KB
static uint8_t mirroring;
static uint8_t bank_to_update;
static uint8_t prg_rom_bank_mode;
static uint8_t chr_a12_inversion;

static uint8_t chr_banks[8];
static uint8_t prg_banks[4];

static uint8_t registers[8];
static uint8_t irq_latch;
static uint16_t irq_counter;
static bool irq_enabled;
static bool irq_reload;

void mmc3_reset() {
	mirroring = 0;
	bank_to_update = 0;
	prg_rom_bank_mode = 0;
	chr_a12_inversion = 0;
	
	for (size_t i = 0; i < 8; i++) {
		registers[i] = 0;
		chr_banks[i] = 0;
	}

	prg_banks[0] = 0;
	prg_banks[1] = 1;
	prg_banks[2] = (size_t)ines.prg_rom_size_16k_chunks * 2 - 2;
	prg_banks[3] = (size_t)ines.prg_rom_size_16k_chunks * 2 - 1;

	irq_latch = 0;
	irq_counter = 0;
	irq_enabled = false;
	irq_reload = false;
}

void mmc3_save_state(void* stream, stream_writer write) {
	write(ram, sizeof(ram), 1, stream);
	write(&mirroring, sizeof(mirroring), 1, stream);
	write(&bank_to_update, sizeof(bank_to_update), 1, stream);
	write(&prg_rom_bank_mode, sizeof(prg_rom_bank_mode), 1, stream);
	write(&chr_a12_inversion, sizeof(chr_a12_inversion), 1, stream);
	write(chr_banks, sizeof(chr_banks), 1, stream);
	write(prg_banks, sizeof(prg_banks), 1, stream);
	write(registers, sizeof(registers), 1, stream);
	write(&irq_latch, sizeof(irq_latch), 1, stream);
	write(&irq_enabled, sizeof(irq_enabled), 1, stream);
	write(&irq_reload, sizeof(irq_reload), 1, stream);
}

void mmc3_load_state(void* stream, stream_reader read) {
	read(ram, sizeof(ram), 1, stream);
	read(&mirroring, sizeof(mirroring), 1, stream);
	read(&bank_to_update, sizeof(bank_to_update), 1, stream);
	read(&prg_rom_bank_mode, sizeof(prg_rom_bank_mode), 1, stream);
	read(&chr_a12_inversion, sizeof(chr_a12_inversion), 1, stream);
	read(chr_banks, sizeof(chr_banks), 1, stream);
	read(prg_banks, sizeof(prg_banks), 1, stream);
	read(registers, sizeof(registers), 1, stream);
	read(&irq_latch, sizeof(irq_latch), 1, stream);
	read(&irq_enabled, sizeof(irq_enabled), 1, stream);
	read(&irq_reload, sizeof(irq_reload), 1, stream);
}

static inline uint16_t ppu_addr_to_ciram_addr(uint16_t ppuaddr) {
	switch (mirroring) {
		case 0:
			// vertical
			ines.ppuaddress_ciram_a10_shift_count = 10;
			break;
		case 1:
			// horizontal
			ines.ppuaddress_ciram_a10_shift_count = 11;
			break;
	}
	uint16_t ciram_address = (ppuaddr & 0x3FF) | (((ppuaddr >> ines.ppuaddress_ciram_a10_shift_count) & 1) << 10);
	return ciram_address;
}

void mmc3_scanline() {
	if (irq_counter == 0 || irq_reload) {
		irq_counter = irq_latch;
		irq_reload = false;
	} else {
		irq_counter--;
	}

	if (irq_counter == 0 && irq_enabled) {
		irq6502();
	}
}

uint8_t mmc3_ppuRead(uint16_t address) {
	if (address & BIT_13) {
		// CIRAM Enabled
		return ciram[ppu_addr_to_ciram_addr(address)];
	}

	size_t bank = 0;
	if (address >= 0x0000 && address <= 0x03FF) {
		bank = chr_banks[0];
	} else if (address >= 0x0400 && address <= 0x07FF) {
		bank = chr_banks[1];
	} else if (address >= 0x0800 && address <= 0x0BFF) {
		bank = chr_banks[2];
	} else if (address >= 0x0C00 && address <= 0x0FFF) {
		bank = chr_banks[3];
	} else if (address >= 0x1000 && address <= 0x13FF) {
		bank = chr_banks[4];
	} else if (address >= 0x1400 && address <= 0x17FF) {
		bank = chr_banks[5];
	} else if (address >= 0x1800 && address <= 0x1BFF) {
		bank = chr_banks[6];
	} else if (address >= 0x1C00 && address <= 0x1FFF) {
		bank = chr_banks[7];
	}
	return ines.chr_rom[bank * 1024 + (size_t)(address & 0x3FF)];
}

void mmc3_ppuWrite(uint16_t address, uint8_t value) {
	if (address & BIT_13) {
		// CIRAM Enabled
		ciram[ppu_addr_to_ciram_addr(address)] = value;
	} else if (address < 0x2000 && ines.is_8k_chr_ram) {
		ines.chr_rom[address] = value;
	}
}

uint8_t mmc3_cpuRead(uint16_t address) {
	if (address >= 0x6000 && address <= 0x7FFF) {
		return ram[address & 0x1FFF];
	}

	uint8_t bank = 0;
	if (address >= 0x8000 && address <= 0x9FFF) {
		bank = prg_banks[0];
	} else if (address >= 0xA000 && address <= 0xBFFF) {
		bank = prg_banks[1];
	} else if (address >= 0xC000 && address <= 0xDFFF) {
		bank = prg_banks[2];
	} else if (address >= 0xE000 && address <= 0xFFFF) {
		bank = prg_banks[3];
	}

	return ines.prg_rom[(size_t)bank * 0x2000 + (size_t)(address & 0x1FFF)];
}

void mmc3_cpuWrite(uint16_t address, uint8_t value) {
	bool address_even = (address & 1) == 0;

	if (address >= 0x6000 && address <= 0x7FFF) {
		ram[address & 0x1FFF] = value;
	} else if (address >= 0x8000 && address <= 0x9FFF) {
		if (address_even) {
			bank_to_update = value & 0b111;
			prg_rom_bank_mode = (value & 0x40);
			chr_a12_inversion = (value & 0x80);
		} else {
			registers[bank_to_update] = value;

			if (chr_a12_inversion) {
				chr_banks[0] = registers[2];
				chr_banks[1] = registers[3];
				chr_banks[2] = registers[4];
				chr_banks[3] = registers[5];
				chr_banks[4] = registers[0] & 0xFE;
				chr_banks[5] = (registers[0] & 0xFE) + 1;
				chr_banks[6] = registers[1] & 0xFE;
				chr_banks[7] = (registers[1] & 0xFE) + 1;
			} else {
				chr_banks[0] = registers[0] & 0xFE;
				chr_banks[1] = (registers[0] & 0xFE) + 1;
				chr_banks[2] = (registers[1] & 0xFE);
				chr_banks[3] = (registers[1] & 0xFE) + 1;
				chr_banks[4] = registers[2];
				chr_banks[5] = registers[3];
				chr_banks[6] = registers[4];
				chr_banks[7] = registers[5];
			}

			size_t num_8k_prg_banks = (size_t)ines.prg_rom_size_16k_chunks * 2;
			prg_banks[0] = prg_rom_bank_mode ? (num_8k_prg_banks - 2) : (registers[6] & 0x3F);
			prg_banks[1] = registers[7] & 0x3F;
			prg_banks[2] = prg_rom_bank_mode ? (registers[6] & 0x3F) : (num_8k_prg_banks - 2);
		}
	} else if (address >= 0xA000 && address <= 0xBFFF) {
		if (address_even) {
			mirroring = value & 1;
		} else {
			// Skip PRG RAM protect register
		}
	} else if (address >= 0xC000 && address <= 0xDFFF) {
		if (address_even) {
			irq_latch = value;
		} else {
			irq_counter = 0;
			irq_reload = true;
		}
	} else if (address >= 0xE000 && address <= 0xFFFF) {
		if (address_even) {
			irq_enabled = false;
		} else {
			irq_enabled = true;
		}
	}
}
