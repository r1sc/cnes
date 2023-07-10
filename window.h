#pragma once

#include <Windows.h>
#include <stdbool.h>
#include <stdint.h>

extern HWND hwnd;
extern HDC window_dc;

extern bool needs_resize;
extern int new_size;
extern unsigned int new_width;
extern unsigned int new_height;

#define NES_VK_RIGHT (1 << 7)
#define NES_VK_LEFT (1 << 6)
#define NES_VK_DOWN (1 << 5)
#define NES_VK_UP (1 << 4)
#define NES_VK_START (1 << 3)
#define NES_VK_SELECT (1 << 2)
#define NES_VK_B 2
#define NES_VK_A 1

extern unsigned int keymap[8];
extern uint8_t keysdown;

void create_window();