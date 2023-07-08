#include <Windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <gl/GL.h>

#include "window.h"
#include "fake6502.h"
#include "ines.h"
#include "ppu.h"
#include "cartridge.h"
#include "bit.h"
#include "NROM.h"
#include "UNROM.h"
#include "disasm.h"

bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

ines_t ines = { 0 };
uint8_t ciram[2048];
uint8_t cpuram[2048];

uint8_t controller_status = 0xFF;

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
		//controller_status = (glfwGetKey(window, GLFW_KEY_RIGHT) << 7)
		//	| (glfwGetKey(window, GLFW_KEY_LEFT) << 6)
		//	| (glfwGetKey(window, GLFW_KEY_DOWN) << 5)
		//	| (glfwGetKey(window, GLFW_KEY_UP) << 4)
		//	| (glfwGetKey(window, GLFW_KEY_S) << 3) // Start
		//	| (glfwGetKey(window, GLFW_KEY_A) << 2) // Select
		//	| (glfwGetKey(window, GLFW_KEY_Z) << 1) // B
		//	| (glfwGetKey(window, GLFW_KEY_X) << 0); // A

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

pixformat_t framebuffer[256 * 240];
size_t cpu_timer = 0;

void tick_frame() {
	scanline = -1;
	while (scanline <= 260) {
		dot = 0;
		while (dot <= 340) {
			if (cpu_timer == 0) {
				step6502();
				cpu_timer = clockticks6502 * 3;
			} else {
				cpu_timer--;
			}

			tick();
			dot++;
		}
		scanline++;
	}
}

void load_ines(char* path) {
	if (ines.prg_rom_banks != NULL) {
		free_ines(&ines);
	}
	
	read_ines(path, &ines);

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

	reset6502();
}

int APIENTRY WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nShowCmd
) {
	load_ines("smb.nes");

	/*disassembler_offset = 0x8000;
	for (int i = 0; i < 20; i++) {
		disassemble();
	}*/

	create_window();

	glEnable(GL_TEXTURE_2D);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 240, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);

	ULONGLONG last = GetTickCount64();
	ULONGLONG now = last;
	ULONGLONG accum = 0;
	ULONGLONG dt = 16;

	MSG msg;
	bool running = true;
	while (running) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = false;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		now = GetTickCount64();
		ULONGLONG delta = now - last;
		last = now;
		accum += delta;

		bool needs_rerender = false;
		while (accum >= dt) {
			tick_frame();
			needs_rerender = true;
			accum -= dt;
		}

		if (needs_rerender) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);
			glBegin(GL_QUADS);
			glTexCoord2i(0, 0); glVertex2i(-1, 1);
			glTexCoord2i(1, 0); glVertex2i(1, 1);
			glTexCoord2i(1, 1); glVertex2i(1, -1);
			glTexCoord2i(0, 1); glVertex2i(-1, -1);
			glEnd();

			SwapBuffers(window_dc);
		}

		Sleep(0);
	}

	free_ines(&ines);
}