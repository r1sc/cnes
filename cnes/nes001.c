#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/cnes.h"
#include "bit.h"
#include "fake6502.h"
#include "NROM.h"
#include "UNROM.h"
#include "MMC1.h"

ines_t ines = { 0 };
bool rom_loaded = false;

cart_reset cartridge_reset;
bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

uint8_t ciram[2048];
uint8_t cpuram[2048];
uint8_t controller_status[2] = { 0xFF, 0xFF };

uint8_t buttons_down[2];

uint8_t read6502(uint16_t address) {
	if (address == 0x4016 || address == 0x4017) {
		uint8_t controller_id = address & 1;
		uint8_t value = controller_status[controller_id] & 1;
		controller_status[controller_id] >>= 1;
		return value;
	} else if ((address >= 0x4000 && address <= 0x4013) || address == 0x4015 || address == 0x4017) {
		// APU
		return 0;
	}

	bool cpu_a15 = (address & BIT_15) != 0;
	bool cpu_a14 = (address & BIT_14) != 0;
	bool cpu_a13 = (address & BIT_13) != 0;
	bool romsel = cpu_a15;

	if (!romsel) {
		bool ppu_cs = !cpu_a14 && cpu_a13;
		bool cpu_ram_cs = !cpu_a14 && !cpu_a13;

		if (ppu_cs) {
			return cpu_ppu_bus_read(address & 7);
		} else if (cpu_ram_cs) {
			return cpuram[address & 0x7FF];
		}
	}
	return cartridge_cpuRead(address);
}

extern size_t cpu_timer;

void write6502(uint16_t address, uint8_t value) {
	if (address == 0x4014) {
		// DMA
		uint16_t page = value << 8;
		for (uint16_t i = 0; i < 256; i++) {
			cpu_ppu_bus_write(4, read6502(page | i));
		}
		cpu_timer += 513;
	} else if (address == 0x4016) {
		controller_status[0] = buttons_down[0];
		controller_status[1] = buttons_down[1];
	} else if ((address >= 0x4000 && address <= 0x4013) || address == 0x4015 || address == 0x4017) {
		;
	} else {
		bool cpu_a15 = (address & BIT_15) != 0;
		bool cpu_a14 = (address & BIT_14) != 0;
		bool cpu_a13 = (address & BIT_13) != 0;
		bool romsel = cpu_a15;

		if (!romsel) {
			bool ppu_cs = !cpu_a14 && cpu_a13;
			bool cpu_ram_cs = !cpu_a14 && !cpu_a13;

			if (ppu_cs) {
				cpu_ppu_bus_write(address & 7, value);
			} else if (cpu_ram_cs) {
				cpuram[address & 0x7FF] = value;
			}
		} else {
			cartridge_cpuWrite(address, value);
		}
	}
}

void reset_machine() {
	reset6502();
	cartridge_reset();
	clockticks6502 = 0;
	cpu_timer = 0;
	for (size_t i = 0; i < 256 * 240; i++) {
		framebuffer[i].r <<= 1;
		framebuffer[i].g <<= 1;
		framebuffer[i].b <<= 1;
	}

	for (size_t i = 0; i < sizeof(ciram); i++) {
		ciram[i] <<= 1;
	}
	for (size_t i = 0; i < sizeof(cpuram); i++) {
		cpuram[i] <<= 1;
	}
}


typedef struct {
	char nes[4];
	uint8_t prg_rom_16k_chunks;
	uint8_t chr_rom_8k_chunks;
	uint8_t flags[5];
	char padding[5];
} ines_header_t;

void read_ines(const char* path) {
	FILE* f = fopen(path, "rb");

	ines_header_t header;
	fread(&header, sizeof(ines_header_t), 1, f);

	char* diskdude = (char*)&header.flags[1];
	if (strncmp(diskdude, "DiskDude!", 9) == 0) {
		ines.mapper_number = header.flags[0] >> 4;
	} else {
		ines.mapper_number = (header.flags[0] >> 4) | (header.flags[1] & 0xF0);
	}

	uint8_t a10 = header.flags[0] & 1;
	ines.ppuaddress_ciram_a10_shift_count = (a10 == 0) ? 11 : 10;

	bool has_trainer = (header.flags[0] & 4) == 4;
	if (has_trainer) fseek(f, 512, SEEK_CUR);

	ines.prg_rom_size_16k_chunks = header.prg_rom_16k_chunks;
	ines.prg_rom_banks = (PRG_ROM_BANK*)calloc(header.prg_rom_16k_chunks, 16384);
	if (!ines.prg_rom_banks) exit(1);

	fread(ines.prg_rom_banks, 16384, header.prg_rom_16k_chunks, f);

	ines.chr_rom_size_8k_chunks = header.chr_rom_8k_chunks;
	size_t chr_rom_size = header.chr_rom_8k_chunks == 0 ? 0x4000 : 8192 * (size_t)header.chr_rom_8k_chunks;
	ines.chr_rom = (uint8_t*)malloc(chr_rom_size);
	if (!ines.chr_rom) exit(1);
	fread(ines.chr_rom, 1, chr_rom_size, f);

	fclose(f);

	rom_loaded = true;
}

void free_ines() {
	rom_loaded = false;
	free(ines.prg_rom_banks);
	free(ines.chr_rom);
}

void load_ines(char* path) {
	if (ines.prg_rom_banks != NULL) {
		free_ines();
	}

	read_ines(path);

	if (ines.mapper_number == 0) {
		cartridge_reset = nrom_reset;
		cartridge_cpuRead = nrom_cpuRead;
		cartridge_cpuWrite = nrom_cpuWrite;
		cartridge_ppuRead = nrom_ppuRead;
		cartridge_ppuWrite = nrom_ppuWrite;
	} else if (ines.mapper_number == 1) {
		cartridge_reset = mmc1_reset;
		cartridge_cpuRead = mmc1_cpuRead;
		cartridge_cpuWrite = mmc1_cpuWrite;
		cartridge_ppuRead = mmc1_ppuRead;
		cartridge_ppuWrite = mmc1_ppuWrite;
	} else if (ines.mapper_number == 2) {
		cartridge_reset = unrom_reset;
		cartridge_cpuRead = unrom_cpuRead;
		cartridge_cpuWrite = unrom_cpuWrite;
		cartridge_ppuRead = unrom_ppuRead;
		cartridge_ppuWrite = unrom_ppuWrite;
	}

	reset_machine();
}