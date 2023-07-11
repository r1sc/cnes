#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "ines.h"

ines_t ines = { 0 };
bool rom_loaded = false;

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

	char* diskdude = (char*)& header.flags[1];
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