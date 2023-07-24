#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/cnes.h"
#include "apu.h"
#include "ppu.h"
#include "fake6502.h"
#include "mappers/NROM.h"
#include "mappers/UNROM.h"
#include "mappers/MMC1.h"

ines_t ines = { 0 };
bool rom_loaded = false;

cart_reset cartridge_reset;
bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

uint8_t ciram[2048];
uint8_t cpuram[2048];
uint8_t controller_status[2] = { 0, 0 };

uint8_t read6502(uint16_t address) {
	if (address == 0x4016 || address == 0x4017) {
		uint8_t controller_id = address & 1;
		uint8_t value = controller_status[controller_id] & 1;
		controller_status[controller_id] >>= 1;
		return value;
	} else if ((address >= 0x4000 && address <= 0x4013) || address == 0x4015 || address == 0x4017) {
		// APU
		return apu_read(address);
	} else if (address >= 0x4000) {
		// Cart
		return cartridge_cpuRead(address);
	} else if (address >= 0x2000) {
		// PPU
		return cpu_ppu_bus_read(address & 7);
	} else {
		// CPU
		return cpuram[address & 0x7FF];
	}
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
		apu_write(address, value);
	} else if (address >= 0x4000) {
		// Cart
		cartridge_cpuWrite(address, value);
	} else if (address >= 0x2000) {
		// PPU
		cpu_ppu_bus_write(address & 7, value);
	} else {
		// CPU
		cpuram[address & 0x7FF] = value;
	}
}

void reset_machine() {

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

	apu_reset();
	ppu_reset();
	cartridge_reset();

	clockticks6502 = 0;
	cpu_timer = 0;
	reset6502();
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

	uint8_t nFileType = 1;
	if ((header.flags[2] & 0x0C) == 0x08) nFileType = 2;

	uint8_t a10 = header.flags[0] & 1;
	ines.ppuaddress_ciram_a10_shift_count = (a10 == 0) ? 11 : 10;

	bool has_trainer = (header.flags[0] & 4) == 4;
	if (has_trainer) fseek(f, 512, SEEK_CUR);

	ines.prg_rom_size_16k_chunks = header.prg_rom_16k_chunks;
	ines.prg_rom = (uint8_t*)malloc((size_t)header.prg_rom_16k_chunks * 16384);
	if (!ines.prg_rom) exit(1);

	fread(ines.prg_rom, 16384, header.prg_rom_16k_chunks, f);

	ines.chr_rom_size_8k_chunks = header.chr_rom_8k_chunks;
	ines.is_8k_chr_ram = header.chr_rom_8k_chunks == 0;
	if (ines.is_8k_chr_ram) {
		ines.chr_rom_size_8k_chunks = 1;
	}

	ines.chr_rom = malloc((size_t)ines.chr_rom_size_8k_chunks * 8192);
	if (!ines.chr_rom) exit(1);

	if (!ines.is_8k_chr_ram) {
		fread(ines.chr_rom, 8192, ines.chr_rom_size_8k_chunks, f);
	}

	fclose(f);

	rom_loaded = true;
}

void free_ines() {
	if (rom_loaded) {
		rom_loaded = false;
		free(ines.chr_rom);
		free(ines.prg_rom);
	}
}

void load_ines(char* path) {
	if (rom_loaded) {
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