#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "ines.h"


typedef struct {
	char nes[4];
	uint8_t prg_rom_16k_chunks;
	uint8_t chr_rom_8k_chunks;
	uint8_t flags[5];
	char padding[5];
} ines_header_t;

void read_ines(const char* path, ines_t* ines) {
	FILE* f = fopen(path, "rb");

	ines_header_t header;
	fread(&header, sizeof(ines_header_t), 1, f);

	uint8_t a10 = header.flags[0] & 1;
	ines->ppuaddress_ciram_a10_shift_count = (a10 == 0) ? 11 : 10;

	bool has_trainer = (header.flags[0] & 4) == 4;
	if (has_trainer) fseek(f, 512, SEEK_CUR);

	size_t prg_rom_size = 16384 * (size_t)header.prg_rom_16k_chunks;
	ines->prg_rom = (uint8_t*)malloc(prg_rom_size);
	if (!ines->prg_rom) exit(1);
	fread(ines->prg_rom, 1, prg_rom_size, f);

	size_t chr_rom_size = 8192 * (size_t)header.chr_rom_8k_chunks;
	ines->chr_rom = (uint8_t*)malloc(chr_rom_size);
	if (!ines->chr_rom) exit(1);
	fread(ines->chr_rom, 1, chr_rom_size, f);

	fclose(f);
}

void free_ines(ines_t* ines) {
	free(ines->prg_rom);
	free(ines->chr_rom);
}