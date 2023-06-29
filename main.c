#include <Windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <gl/GL.h>
#include <GLFW/glfw3.h>

#include "window.h"
#include "fake6502.h"
#include "ines.h"
#include "ppu.h"
#include "cartridge.h"
#include "bit.h"
#include "NROM.h"
#include "disasm.h"

bus_read_t cartridge_cpuRead;
bus_write_t cartridge_cpuWrite;
bus_read_t cartridge_ppuRead;
bus_write_t cartridge_ppuWrite;

ines_t ines;
uint8_t ciram[2048];
uint8_t cpuram[2048];

uint8_t controller_status = 0xFF;

uint8_t read6502(uint16_t address) {
	if (address == 0x4016) {
		uint8_t value = controller_status & 1;
		controller_status >>= 1;
		return value;
	} else if (address >= 0x4000 && address <= 0x401f) {
		// APU
		return 0;
	}

	bool cpu_a15 = (address & BIT_15) != 0;
	bool cpu_a14 = (address & BIT_14) != 0;
	bool cpu_a13 = (address & BIT_13) != 0;
	bool phase2 = true;

	bool romsel = cpu_a15 && phase2;
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

static GLFWwindow* window;
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
		controller_status = (glfwGetKey(window, GLFW_KEY_RIGHT) << 7)
			| (glfwGetKey(window, GLFW_KEY_LEFT) << 6)
			| (glfwGetKey(window, GLFW_KEY_DOWN) << 5)
			| (glfwGetKey(window, GLFW_KEY_UP) << 4)
			| (glfwGetKey(window, GLFW_KEY_A) << 3) // Start
			| (glfwGetKey(window, GLFW_KEY_S) << 2) 
			| (glfwGetKey(window, GLFW_KEY_Z) << 1)
			| (glfwGetKey(window, GLFW_KEY_X) << 0);

	} else if (address >= 0x4000 && address <= 0x401f) {
		;
	} else {
		bool cpu_a15 = (address & BIT_15) != 0;
		bool cpu_a14 = (address & BIT_14) != 0;
		bool cpu_a13 = (address & BIT_13) != 0;
		bool phase2 = true;

		bool romsel = cpu_a15 && phase2;
		if (!romsel) {
			bool ppu_cs = !cpu_a14 && cpu_a13;
			bool cpu_ram_cs = !cpu_a14 && !cpu_a13;

			if (ppu_cs) {
				cpu_ppu_bus_write(address & 7, value);
			} else if (cpu_ram_cs) {
				cpuram[address & 0x7FF] = value;
			}
		}
		else {
			cartridge_cpuWrite(address, value);
		}
	}
}

pixformat_t framebuffer[256 * 256];
bool hold_clock = false;

void main() {
	read_ines("smb.nes", &ines);

	cartridge_cpuRead = nrom_cpuRead;
	cartridge_cpuWrite = nrom_cpuWrite;
	cartridge_ppuRead = nrom_ppuRead;
	cartridge_ppuWrite = nrom_ppuWrite;

	reset6502();

	disassembler_offset = 0x8000;
	for (int i = 0; i < 20; i++) {
		disassemble();
	}

	glfwInit();
	window = glfwCreateWindow(512, 512, "cnes", NULL, NULL);
	glfwMakeContextCurrent(window);

	glEnable(GL_TEXTURE_2D);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);

	DWORD last = GetTickCount();
	DWORD accum = 0;

	while (!glfwWindowShouldClose(window)) {
		DWORD now = GetTickCount();
		DWORD delta = now - last;
		last = now;
		accum += delta;

		bool needs_rerender = false;
		while (accum >= 16) {
			tick_frame();
			needs_rerender = true;
			accum -= 16;
		}

		if (needs_rerender) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);
			glBegin(GL_QUADS);
			glTexCoord2i(0, 0); glVertex2i(-1, 1);
			glTexCoord2i(1, 0); glVertex2i(1, 1);
			glTexCoord2i(1, 1); glVertex2i(1, -1);
			glTexCoord2i(0, 1); glVertex2i(-1, -1);
			glEnd();

			/* Swap front and back buffers */
			glfwSwapBuffers(window);
		}

		/* Poll for and process events */
		glfwPollEvents();
	}
	glfwTerminate();

	free_ines(&ines);
}