#pragma once

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} pixformat_t;

extern pixformat_t framebuffer[256 * 240];
extern uint8_t buttons_down[2];
extern void write_audio_sample(int scanline, int16_t sample);

void load_ines(char* path);
void free_ines();
void reset_machine();
void tick_frame();