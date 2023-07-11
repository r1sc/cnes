#include <stdint.h>
#include <stdbool.h>

#include "bit.h"
#include "fake6502.h"
#include "ppu.h"
#include "cartridge.h"
#include "NROM.h"
#include "UNROM.h"
#include "ines.h"

bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

uint8_t ciram[2048];
uint8_t cpuram[2048];
uint8_t controller_status = 0xFF;
extern uint8_t keysdown;

uint8_t read6502(uint16_t address) {
	if (address == 0x4016) {
		uint8_t value = controller_status & 1;
		controller_status >>= 1;
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

//static GLFWwindow* window;
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
		controller_status = keysdown;
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

void load_ines(char* path) {
	if (ines.prg_rom_banks != NULL) {
		free_ines();
	}

	read_ines(path);

	if (ines.mapper_number == 0) {

		cartridge_cpuRead = nrom_cpuRead;
		cartridge_cpuWrite = nrom_cpuWrite;
		cartridge_ppuRead = nrom_ppuRead;
		cartridge_ppuWrite = nrom_ppuWrite;
	} else if (ines.mapper_number == 2) {
		cartridge_cpuRead = unrom_cpuRead;
		cartridge_cpuWrite = unrom_cpuWrite;
		cartridge_ppuRead = unrom_ppuRead;
		cartridge_ppuWrite = unrom_ppuWrite;
	}

	reset_machine();
}