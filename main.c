#include <Windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <gl/GL.h>
#include <dwmapi.h>

#include "window.h"
#include "fake6502.h"
#include "ines.h"
#include "ppu.h"
#include "cartridge.h"
#include "bit.h"
#include "NROM.h"

bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

ines_t ines;
uint8_t ciram[2048];
uint8_t cpuram[2048];

uint8_t read6502(uint16_t address) {
	bool cpu_a15 = (address & BIT_15) != 0;
	bool cpu_a14 = (address & BIT_14) != 0;
	bool cpu_a13 = (address & BIT_13) != 0;
	bool phase2 = true;

	bool romsel = cpu_a15 && phase2;
	bool ppu_cs = !cpu_a14 && cpu_a13;
	bool cpu_ram_cs = !cpu_a14 && !cpu_a13;

	if (ppu_cs) {
		return cpu_ppu_bus_read(address & 7);
	} else if (cpu_ram_cs) {
		return cpuram[address & 0x7FF];
	}
	return cartridge_cpuRead(address);
}

void write6502(uint16_t address, uint8_t value) {
	if (address == 0x4014) {
		// DMA
		uint16_t page = value << 8;
		for (uint16_t i = 0; i < 256; i++) {
			cpu_ppu_bus_write(4, read6502(page | i));
		}
	} else {
		bool cpu_a15 = (address & BIT_15) != 0;
		bool cpu_a14 = (address & BIT_14) != 0;
		bool cpu_a13 = (address & BIT_13) != 0;
		bool phase2 = true;

		bool romsel = cpu_a15 && phase2;
		bool ppu_cs = !cpu_a14 && cpu_a13;
		bool cpu_ram_cs = !cpu_a14 && !cpu_a13;

		if (ppu_cs) {
			cpu_ppu_bus_write(address & 7, value);
		} else if (cpu_ram_cs) {
			cpuram[address & 0x7FF] = value;
		} else {
			cartridge_cpuWrite(address, value);
		}
	}
}

pixformat_t framebuffer[256 * 256];
bool hold_clock = false;

void main() {
	read_ines("donkey kong.nes", &ines);

	cartridge_cpuRead = nrom_cpuRead;
	cartridge_cpuWrite = nrom_cpuWrite;
	cartridge_ppuRead = nrom_ppuRead;
	cartridge_ppuWrite = nrom_ppuWrite;

	reset6502();

	create_window();
	ShowWindow(hwnd, SW_SHOW);

	glEnable(GL_TEXTURE_2D);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);

	MSG msg;
	bool running = true;
	while (running) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				running = false;
			} else if (msg.message == WM_SIZE) {
				UINT width = LOWORD(msg.lParam);
				UINT height = HIWORD(msg.lParam);
				glViewport(0,0,width, height);
			}
		}

		tick_frame();

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);

		glBegin(GL_QUADS);
		glTexCoord2i(0, 0); glVertex2i(-1, 1);
		glTexCoord2i(1, 0); glVertex2i(1, 1);
		glTexCoord2i(1, 1); glVertex2i(1, -1);
		glTexCoord2i(0, 1); glVertex2i(-1, -1);
		glEnd();


		SwapBuffers(window_dc);
		Sleep(0);
	}

	step6502();

	free_ines(&ines);
}