#pragma once
#include <stdint.h>

typedef uint8_t PRG_ROM_BANK[16384];

typedef struct {
	uint8_t mapper_number;
	PRG_ROM_BANK* prg_rom_banks;
	uint8_t prg_rom_size_16k_chunks;
	uint8_t* chr_rom;
	uint8_t chr_rom_size_8k_chunks;
	uint8_t ppuaddress_ciram_a10_shift_count;
} ines_t;

void read_ines(const char* path, ines_t* ines);
void free_ines(ines_t* ines);