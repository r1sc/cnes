#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "apu.h"

static uint8_t duty_cycles[4] = { 0b10000000, 0b11000000, 0b11110000, 0b00111111 };

typedef struct {
	uint16_t timer;
	uint16_t timer_cur;
	uint16_t reload;
	uint8_t sequence;
	uint8_t sequencer_pos;
	uint8_t current_output;
	bool length_counter_halt;
	bool constant_volume;
	uint8_t volume;
} apu_pulse_t;

void pulse_write_reg(apu_pulse_t* pulse, uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
		{
			uint8_t duty_cycle_index = (value >> 6) & 0b11;
			pulse->sequence = duty_cycles[duty_cycle_index];

			pulse->length_counter_halt = (value & 0b00100000) != 0;
			pulse->constant_volume = (value & 0b00010000) != 0;
			pulse->volume = value & 0b1111;
		}
		break;
		case 1:
			// APU Sweep, do later
			break;
		case 2:
			pulse->reload = (pulse->reload & 0x700) | value;
			break;
		case 3:
			pulse->reload = (pulse->reload & 0xFF) | ((value & 0b111) << 8);
			pulse->timer_cur = pulse->reload;
			break;
	}
}

void pulse_tick(apu_pulse_t* pulse) {
	if (pulse->timer_cur == 0) {
		pulse->timer_cur = pulse->reload;
		pulse->current_output = (pulse->sequence >> pulse->sequencer_pos) & 1;

		if (pulse->sequencer_pos == 0) {
			pulse->sequencer_pos = 7;
		} else {
			pulse->sequencer_pos--;
		}

	} else {
		pulse->timer_cur -= 1;
	}
}

static apu_pulse_t pulse1 = { 0 };
static apu_pulse_t pulse2 = { 0 };

bool pulse1_enabled = false;
bool pulse2_enabled = false;
bool triangle_enabled = false;
bool dmc_enabled = false;

void apu_write(uint16_t address, uint8_t value) {
	if (address >= 0x4000 && address <= 0x4003) {
		pulse_write_reg(&pulse1, address & 0b11, value);
	} else if (address >= 0x4004 && address <= 0x4007) {
		pulse_write_reg(&pulse2, address & 0b11, value);
	} else if (address == 0x4015) {
		// Status
		pulse1_enabled = value & 1;
		pulse2_enabled = value & 2;
		triangle_enabled = value & 4;
		dmc_enabled = value & 8;
	}
}

int16_t frame_samples[262];
int16_t acc = 0;

#define WINDOW_LEN 8
static int16_t window[WINDOW_LEN];
static int window_pos = 0;
static int oout = 0;
static int16_t prev_sample = 0;

void apu_tick(uint16_t scanline) {

	if (pulse1_enabled) {
		pulse_tick(&pulse1);
	}
	if (pulse2_enabled) {
		pulse_tick(&pulse2);
	}

	int num_enabled = 0;
	int16_t frame_sample = 0;

	if (pulse1_enabled) {
		frame_sample += pulse1.current_output ? 15000 : -15000;
		num_enabled++;
	}
	if (pulse2_enabled) {
		frame_sample += pulse2.current_output ? 15000 : -15000;
		num_enabled++;
	}

	if (num_enabled > 0) {
		frame_sample /= num_enabled;
	}

	window[window_pos] = frame_sample;
	
	int current = 0;
	for (size_t i = 0; i < WINDOW_LEN; i++) {
		current += window[i];
	}

	window[window_pos] = (int16_t)(current / WINDOW_LEN);
	/*frame_samples[scanline] = window[window_pos++];*/

	if (window_pos >= WINDOW_LEN) {
		window_pos = 0;
	}

	oout += (frame_sample - prev_sample);
	oout -= (oout / 512);
	prev_sample = oout;

	frame_samples[scanline] = oout;
}
