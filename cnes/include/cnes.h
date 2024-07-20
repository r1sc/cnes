#ifndef _CNES_H_
#define _CNES_H_

#include "../stream.h"

#define CNES_LOAD_NO_ERR 0
#define CNES_LOAD_MAPPER_NOT_SUPPORTED 1

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	} pixformat_t;

	extern pixformat_t framebuffer[256 * 240];
	extern uint8_t buttons_down[2];
	extern void write_audio_sample(int scanline, int16_t sample);
	extern uint8_t* get_8k_chr_ram(uint8_t num_8k_chunks);

	int load_ines(const char* data);
	void reset_machine();
	void tick_frame();

	void save_state(void* stream, stream_writer write);
	void load_state(void* stream, stream_reader read);

#ifdef __cplusplus
}
#endif

#endif