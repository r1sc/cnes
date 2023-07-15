#include <stdint.h>
#include <assert.h>
#include "apu.h"

// 12 bits
static uint16_t pulse_1_timer = 0;
static uint16_t pulse_1_timer_latch = 254;
static int8_t pulse_1_sample = 0;

static uint8_t sequence_template = 0b01111000;
static uint8_t sequence = 0b01111000;
static uint8_t seq_steps = 8;

static void pulse_1_waveform_clock() {
	pulse_1_sample = (sequence & 0x80) != 0 ? 127 : -127;
	sequence <<= 1;
	if (seq_steps == 0) {
		sequence = sequence_template;
		seq_steps = 8;
	} else {
		seq_steps--;
	}
}

static void pulse_1_timer_tick() {
	if (pulse_1_timer == 0) {
		pulse_1_timer = pulse_1_timer_latch;
		pulse_1_waveform_clock();
	} else {
		pulse_1_timer--;
	}
}

static size_t mixer_pos = 0;
static int8_t* current_mixer_buffer = NULL;

int waits = 0;
/// <summary>
/// Runs every second CPU cycle
/// </summary>
void tick_apu() {
	pulse_1_timer_tick();
	if (waits == 0) {
		produce_sample();
		waits = 10;
	} else {
		waits--;
	}
}
//
//void produce_sample() {
//	if (current_mixer_buffer == NULL) {
//		current_mixer_buffer = waveout_get_current_buffer();
//		//assert(current_mixer_buffer != NULL);
//		mixer_pos = 0;
//	}
//
//	if (current_mixer_buffer != NULL && mixer_pos < 4096) {
//		current_mixer_buffer[mixer_pos++] = pulse_1_sample;
//		if (mixer_pos == 4096) {
//			waveout_queue_buffer();
//			current_mixer_buffer = NULL;
//		}
//	}
//}