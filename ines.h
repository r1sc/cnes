#pragma once
#include <stdint.h>

typedef struct {
	uint8_t* prg_rom;
	uint8_t prg_rom_size_16k_chunks;
	uint8_t* chr_rom;
	uint8_t chr_rom_size_8k_chunks;
	uint8_t ppuaddress_ciram_a10_shift_count;
} ines_t;

void read_ines(const char* path, ines_t* ines);
void free_ines(ines_t* ines);