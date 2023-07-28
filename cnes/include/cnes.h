#ifndef _CNES_H_
#define _CNES_H_

#include "../stream.h"

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} pixformat_t;

extern pixformat_t framebuffer[256 * 240];
extern uint8_t buttons_down[2];
extern void write_audio_sample(int scanline, int16_t sample);

int load_ines(const char* data);
void free_ines();
void reset_machine();
void tick_frame();

void save_state(void* stream, stream_writer write);
void load_state(void* stream, stream_reader read);

#endif