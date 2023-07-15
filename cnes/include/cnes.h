#pragma once

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} pixformat_t;

extern pixformat_t framebuffer[256 * 240];

void load_ines(char* path);
void reset_machine();
void tick_frame();