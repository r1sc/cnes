#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "include/cnes.h"
#include "apu.h"
#include "ppu.h"
#include "fake6502.h"
#include "mappers/NROM.h"
#include "mappers/UNROM.h"
#include "mappers/MMC1.h"
#include "mappers/MMC2.h"
#include "mappers/ColorDreams.h"
#include "mappers/MMC3.h"

ines_t ines = { 0 };
bool rom_loaded = false;
uint8_t buttons_down[2] = { 0, 0 };

cart_reset cartridge_reset;
cart_save_state cartridge_save_state;
cart_load_state cartridge_load_state;
cart_scanline cartridge_scanline;
bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

uint8_t ciram[2048];
static uint8_t cpuram[2048];
static uint8_t controller_status[2] = { 0, 0 };

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

void read_ines(const char* data) {
	ines_header_t* header = (ines_header_t*)data;

	char* diskdude = (char*)&header->flags[1];
	if (strncmp(diskdude, "DiskDude!", 9) == 0) {
		ines.mapper_number = header->flags[0] >> 4;
	} else {
		ines.mapper_number = (header->flags[0] >> 4) | (header->flags[1] & 0xF0);
	}

	uint8_t nFileType = 1;
	if ((header->flags[2] & 0x0C) == 0x08) nFileType = 2;

	uint8_t a10 = header->flags[0] & 1;
	ines.ppuaddress_ciram_a10_shift_count = (a10 == 0) ? 11 : 10;

	size_t data_offset = sizeof(ines_header_t);
	bool has_trainer = (header->flags[0] & 4) == 4;
	if (has_trainer) data_offset += 512;

	ines.prg_rom_size_16k_chunks = header->prg_rom_16k_chunks;
	ines.prg_rom = (uint8_t*)(data + data_offset);
	if (!ines.prg_rom) exit(1);

	data_offset += 16384 * (size_t)header->prg_rom_16k_chunks;	

	ines.chr_rom_size_8k_chunks = header->chr_rom_8k_chunks;
	ines.is_8k_chr_ram = header->chr_rom_8k_chunks == 0;
	if (ines.is_8k_chr_ram) {
		ines.chr_rom_size_8k_chunks = 1;
	}

	ines.chr_rom = (uint8_t*)(data + data_offset);

	if (ines.is_8k_chr_ram) {
		ines.chr_rom = get_8k_chr_ram(ines.chr_rom_size_8k_chunks);
	}
}

int load_ines(const char* data) {
	read_ines(data);

	cartridge_scanline = NULL;
	if (ines.mapper_number == 0) {
		cartridge_reset = nrom_reset;
		cartridge_save_state = nrom_save_state;
		cartridge_load_state = nrom_load_state;
		cartridge_cpuRead = nrom_cpuRead;
		cartridge_cpuWrite = nrom_cpuWrite;
		cartridge_ppuRead = nrom_ppuRead;
		cartridge_ppuWrite = nrom_ppuWrite;
	} else if (ines.mapper_number == 1) {
		cartridge_reset = mmc1_reset;
		cartridge_save_state = mmc1_save_state;
		cartridge_load_state = mmc1_load_state;
		cartridge_cpuRead = mmc1_cpuRead;
		cartridge_cpuWrite = mmc1_cpuWrite;
		cartridge_ppuRead = mmc1_ppuRead;
		cartridge_ppuWrite = mmc1_ppuWrite;
	} else if (ines.mapper_number == 2) {
		cartridge_reset = unrom_reset;
		cartridge_save_state = unrom_save_state;
		cartridge_load_state = unrom_load_state;
		cartridge_cpuRead = unrom_cpuRead;
		cartridge_cpuWrite = unrom_cpuWrite;
		cartridge_ppuRead = unrom_ppuRead;
		cartridge_ppuWrite = unrom_ppuWrite;
	} else if (ines.mapper_number == 4) {
		cartridge_reset = mmc3_reset;
		cartridge_save_state = mmc3_save_state;
		cartridge_load_state = mmc3_load_state;
		cartridge_cpuRead = mmc3_cpuRead;
		cartridge_cpuWrite = mmc3_cpuWrite;
		cartridge_ppuRead = mmc3_ppuRead;
		cartridge_ppuWrite = mmc3_ppuWrite;
		cartridge_scanline = mmc3_scanline;
	} else if (ines.mapper_number == 9) {
		cartridge_reset = mmc2_reset;
		cartridge_save_state = mmc2_save_state;
		cartridge_load_state = mmc2_load_state;
		cartridge_cpuRead = mmc2_cpuRead;
		cartridge_cpuWrite = mmc2_cpuWrite;
		cartridge_ppuRead = mmc2_ppuRead;
		cartridge_ppuWrite = mmc2_ppuWrite;
	} else if (ines.mapper_number == 11) {
		cartridge_reset = colordreams_reset;
		cartridge_save_state = colordreams_save_state;
		cartridge_load_state = colordreams_load_state;
		cartridge_cpuRead = colordreams_cpuRead;
		cartridge_cpuWrite = colordreams_cpuWrite;
		cartridge_ppuRead = colordreams_ppuRead;
		cartridge_ppuWrite = colordreams_ppuWrite;
	} else {
		return CNES_LOAD_MAPPER_NOT_SUPPORTED;
	}

	rom_loaded = true;

	reset_machine();

	return CNES_LOAD_NO_ERR;
}

void save_state(void* stream, stream_writer write) {
	write(cpuram, sizeof(cpuram), 1, stream);
	write(ciram, sizeof(ciram), 1, stream);
	write(&PPU_state, sizeof(PPU_state), 1, stream);
	
	cartridge_save_state(stream, write);
	
	if (ines.is_8k_chr_ram) {
		write(ines.chr_rom, 8192, sizeof(uint8_t), stream);
	}

	write(&pc, sizeof(pc), 1, stream);
	write(&sp, sizeof(sp), 1, stream);
	write(&a, sizeof(a), 1, stream);
	write(&x, sizeof(x), 1, stream);
	write(&y, sizeof(y), 1, stream);
}

void load_state(void* stream, stream_reader read) {
	read(cpuram, sizeof(cpuram), 1, stream);
	read(ciram, sizeof(ciram), 1, stream);
	read(&PPU_state, sizeof(PPU_state), 1, stream);
	
	cartridge_load_state(stream, read);
	
	if (ines.is_8k_chr_ram) {
		read(ines.chr_rom, 8192, sizeof(uint8_t), stream);
	}

	read(&pc, sizeof(pc), 1, stream);
	read(&sp, sizeof(sp), 1, stream);
	read(&a, sizeof(a), 1, stream);
	read(&x, sizeof(x), 1, stream);
	read(&y, sizeof(y), 1, stream);
}
