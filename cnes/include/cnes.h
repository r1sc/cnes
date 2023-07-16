#pragma once

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} pixformat_t;

extern pixformat_t framebuffer[256 * 240];
extern int16_t frame_samples[262];
extern uint8_t buttons_down[2];

void load_ines(char* path);
void reset_machine();
void tick_frame();