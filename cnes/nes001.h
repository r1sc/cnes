#ifndef _NES001_H_
#define _NES001_H_

#include <stdint.h>
#include <stdbool.h>
#include "stream.h"

typedef struct {
	uint8_t mapper_number;

	uint8_t* prg_rom;
	uint8_t prg_rom_size_16k_chunks;

	uint8_t* chr_rom;
	uint8_t chr_rom_size_8k_chunks;
	bool is_8k_chr_ram;

	uint8_t ppuaddress_ciram_a10_shift_count;
} ines_t;

extern ines_t ines;
extern bool rom_loaded;
extern uint8_t ciram[2048];

typedef uint8_t(*bus_read_t)(uint16_t address);
typedef void(*bus_write_t)(uint16_t address, uint8_t value);
typedef void(*cart_reset)();
typedef void(*cart_save_state)(void* stream, stream_writer write);
typedef void(*cart_load_state)(void* stream, stream_reader read);
typedef void(*cart_scanline)();

extern bus_read_t cartridge_ppuRead;
extern bus_write_t cartridge_ppuWrite;
extern cart_scanline cartridge_scanline;

uint8_t cpu_ppu_bus_read(uint8_t address);
void cpu_ppu_bus_write(uint8_t address, uint8_t value);
void tick_frame();

#endif