#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "apu.h"
#include "fake6502.h"
#include "include/cnes.h"

static uint8_t duty_cycles[4] = { 0b10000000, 0b11000000, 0b11110000, 0b00111111 };

typedef struct {
	uint16_t timer_cur;
	uint16_t reload;
	uint8_t sequence;
	uint8_t sequencer_pos;
	uint8_t current_output;
	uint16_t length_counter;
	bool length_counter_halt;
	bool constant_volume;
	uint8_t volume;
} apu_pulse_t;

uint8_t length_table[] = {	10, 254, 20,  2, 40,  4, 80,  6,
							160,   8, 60, 10, 14, 12, 26, 14,
							12,  16, 24, 18, 48, 20, 96, 22,
							192,  24, 72, 26, 16, 28, 32, 30 };

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
			pulse->length_counter = length_table[(value & 0xF8) >> 3];
			
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
static unsigned int apu_cycle_counter = 0;
static bool five_step_mode = false;
static bool interrupt_inhibit = true;

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

		if (!pulse1_enabled) pulse1.length_counter = 0;
		if (!pulse2_enabled) pulse2.length_counter = 0;
	}
}

void clock_linear_counters() {

}

void clock_length_counters() {
	if (!pulse1.length_counter_halt && pulse1.length_counter > 0) {
		pulse1.length_counter--;
	}

	if (!pulse2.length_counter_halt && pulse2.length_counter > 0) {
		pulse2.length_counter--;
	}
}

void apu_tick(uint16_t scanline) {
	switch (apu_cycle_counter) {
		case 3728:
			clock_linear_counters();
			break;
		case 7456:
			clock_linear_counters();
			clock_length_counters();
			break;
		case 11185:
			clock_linear_counters();
			break;
		case 14914:
			if (!five_step_mode) {
				if (!interrupt_inhibit) {
					irq6502();
				}
				clock_linear_counters();
				clock_length_counters();
			}
			break;
		case 14915:
			if (!five_step_mode) {
				apu_cycle_counter = 0;
			}
			break;
		case 18640:
			if (five_step_mode) {
				clock_linear_counters();
				clock_length_counters();
			}
			break;
		case 18641:
			if (five_step_mode) {
				apu_cycle_counter = 0;
			}
			break;
	}

	if (pulse1_enabled) {
		pulse_tick(&pulse1);
	}
	if (pulse2_enabled) {
		pulse_tick(&pulse2);
	}

	int num_enabled = 0;
	int16_t frame_sample = 0;

	if (pulse1_enabled && pulse1.length_counter > 0 && pulse1.timer_cur >= 8) {
		frame_sample += pulse1.current_output ? 5000 : -5000;
		num_enabled++;
	}
	if (pulse2_enabled && pulse2.length_counter > 0 && pulse2.timer_cur >= 8) {
		frame_sample += pulse2.current_output ? 5000 : -5000;
		num_enabled++;
	}

	if (num_enabled > 0) {
		frame_sample /=  num_enabled;
	}

	write_audio_sample(scanline, frame_sample);
	apu_cycle_counter++;
}
