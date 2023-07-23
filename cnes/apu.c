#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "apu.h"
#include "fake6502.h"
#include "include/cnes.h"

/******* TIMER **********/
typedef struct {
	uint16_t current;
	uint16_t reload;
} timer_t;

static bool timer_tick(timer_t* timer) {
	if (timer->current == 0) {
		timer->current = timer->reload;
		return true;
	} else {
		timer->current--;
		return false;
	}
}

/********* LENGTH COUNTER *********/
typedef struct {
	uint8_t value;
	bool halt;
} lengthcounter_t;

static void clock_length_counter(lengthcounter_t* length_counter) {
	if (!length_counter->halt && length_counter->value > 0) {
		length_counter->value--;
	}
}

/******** ENVELOPE *********/
typedef struct {
	bool start;
	timer_t timer;
	uint8_t decay_level;
	bool constant_volume;
} envelope_t;

static void clock_envelope(envelope_t* envelope, bool loop_flag) {
	if (!envelope->start) {
		if (timer_tick(&envelope->timer)) {
			if (envelope->decay_level > 0) {
				envelope->decay_level--;
			} else if(loop_flag) {
				envelope->decay_level = 15;
			}
		}
	} else {
		envelope->start = false;
		envelope->decay_level = 15;
		envelope->timer.current = envelope->timer.reload;
	}
}

uint8_t envelope_get_volume(envelope_t* envelope) {
	return envelope->constant_volume ? (uint8_t)envelope->timer.reload : envelope->decay_level;
}

/********* PULSE *******/

static uint8_t duty_cycles[4] = { 0b10000000, 0b11000000, 0b11110000, 0b00111111 };

typedef struct {
	timer_t timer;
	lengthcounter_t lengthcounter;
	uint8_t sequence;
	uint8_t sequencer_pos;
	uint8_t current_output;	
	envelope_t envelope;

	bool sweep_enabled;
	uint16_t sweep_divider_current;
	uint16_t sweep_divider_reload;
	bool sweep_reload_flag;
	bool sweep_negate;
	uint8_t sweep_shift_count;
	uint16_t sweep_target_period;

} apu_pulse_t;

static uint8_t length_table[] = { 10, 254, 20,  2, 40,  4, 80,  6,
							160,   8, 60, 10, 14, 12, 26, 14,
							12,  16, 24, 18, 48, 20, 96, 22,
							192,  24, 72, 26, 16, 28, 32, 30 };

static void pulse_calculate_sweep_target(apu_pulse_t* pulse) {
	int change_amount = (int)(pulse->timer.reload >> pulse->sweep_shift_count);
	if (pulse->sweep_negate) {
		change_amount = -change_amount;
	}
	int target_period = (int)pulse->timer.reload + change_amount;
	if (target_period < 0) {
		target_period = 0;
	}
	pulse->sweep_target_period = (uint16_t)target_period;
}

static void pulse_write_reg(apu_pulse_t* pulse, uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
		{
			uint8_t duty_cycle_index = (value >> 6) & 0b11;
			pulse->sequence = duty_cycles[duty_cycle_index];

			pulse->lengthcounter.halt = (value & 0b00100000) != 0;
			pulse->envelope.constant_volume = (value & 0b00010000) != 0;
			pulse->envelope.timer.reload = value & 0b1111;
		}
		break;
		case 1:
			pulse->sweep_enabled = value & 0x80;
			pulse->sweep_divider_reload = (value >> 4) & 0b111;
			pulse->sweep_negate = value & 0b1000;
			pulse->sweep_shift_count = value & 0b111;

			pulse->sweep_reload_flag = true;
			break;
		case 2:
			pulse->timer.reload = (pulse->timer.reload & 0x700) | value;
			//pulse_calculate_sweep_target(pulse);
			break;
		case 3:
			pulse->timer.reload = (pulse->timer.reload & 0xFF) | ((value & 0b111) << 8);
			pulse->timer.current = pulse->timer.reload;
			pulse->lengthcounter.value = length_table[value >> 3];
			pulse->sequencer_pos = 0; // Reset phase
			pulse->envelope.start = true;
			//pulse_calculate_sweep_target(pulse);
			break;
	}
}

static void pulse_tick(apu_pulse_t* pulse, bool sweep_ones_complement) {
	pulse_calculate_sweep_target(pulse);

	if (timer_tick(&pulse->timer)) {
		if (pulse->lengthcounter.value > 0 && pulse->timer.reload >= 8 && pulse->sweep_target_period <= 0x7FF) {
			pulse->current_output = ((pulse->sequence >> pulse->sequencer_pos) & 1) ? envelope_get_volume(&pulse->envelope) : 0;
		} else {
			pulse->current_output = 0;
		}

		if (pulse->sequencer_pos == 0) {
			pulse->sequencer_pos = 7;
		} else {
			pulse->sequencer_pos--;
		}
	}
}

static void clock_sweep_unit(apu_pulse_t* pulse) {
	if (pulse->sweep_divider_current == 0 && pulse->sweep_enabled && pulse->timer.reload >= 8 && pulse->sweep_target_period <= 0x7FF) {
		pulse->timer.reload = pulse->sweep_target_period;
		//pulse_calculate_sweep_target(pulse);
	}
	if (pulse->sweep_divider_current == 0 || pulse->sweep_reload_flag) {
		pulse->sweep_divider_current = pulse->sweep_divider_reload;
		pulse->sweep_reload_flag = false;
	} else {
		pulse->sweep_divider_current--;
	}
}

struct {
	timer_t timer;
	lengthcounter_t lengthcounter;
	uint16_t linear_counter;
	uint16_t linear_counter_reload;
	bool linear_counter_reload_flag;	
	uint8_t current_output;
	uint8_t sequencer_pos;
} triangle = { 0 };

static void triangle_write_reg(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			triangle.linear_counter_reload = value & 0b01111111;
			triangle.lengthcounter.halt = (value & 0x80) == 0x80;
			break;
		case 2:
			triangle.timer.reload = (triangle.timer.reload & 0x700) | value;
			break;
		case 3:
			triangle.lengthcounter.value = length_table[(value & 0xF8) >> 3];
			triangle.timer.reload = (triangle.timer.reload & 0xFF) | ((value & 0b111) << 8);
			triangle.timer.current = triangle.timer.reload;
			triangle.linear_counter_reload_flag = true;
			break;
	}
}

static uint8_t triangle_sequence[] = {
	15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

static void triangle_tick() {
	if (timer_tick(&triangle.timer)) {
		if (triangle.linear_counter > 0 && triangle.lengthcounter.value > 0) {
			triangle.current_output = triangle_sequence[triangle.sequencer_pos];

			if (triangle.sequencer_pos == 31) {
				triangle.sequencer_pos = 0;
			} else {
				triangle.sequencer_pos++;
			}
		}
	}
}

/******** NOISE ************/
static uint16_t noise_period_table[] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };

static struct {
	timer_t timer;
	lengthcounter_t lengthcounter;
	uint8_t period_select;
	uint8_t current_output;

	uint16_t shift_reg;
	bool mode;

	envelope_t envelope;
} noise;

static void noise_reset() {
	noise.shift_reg = 1;
}

static void noise_tick() {
	if (timer_tick(&noise.timer)) {
		uint16_t feedback = (noise.shift_reg & 1) ^ ((noise.mode ? (noise.shift_reg >> 6) : (noise.shift_reg >> 1)) & 1);
		noise.shift_reg >>= 1;
		noise.shift_reg = (noise.shift_reg & 0x3FFF) | (feedback << 14);

		if ((noise.shift_reg & 1) == 1 || noise.lengthcounter.value == 0) {
			noise.current_output = 0;
		} else {
			noise.current_output = envelope_get_volume(&noise.envelope);
		}
	}
}

static void noise_write_reg(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			noise.lengthcounter.halt = value & 0b100000;
			noise.envelope.constant_volume = value & 0b10000;
			noise.envelope.timer.reload = value & 0b1111;
			break;
		case 2:
			noise.timer.reload = noise_period_table[value & 0b1111];
			noise.timer.current = noise.timer.reload;
			break;
		case 3:
			noise.lengthcounter.value = length_table[value >> 3];
			noise.envelope.start = true;
			break;
	}
}

static apu_pulse_t pulse1 = { 0 };
static apu_pulse_t pulse2 = { 0 };
static unsigned int apu_cycle_counter = 0;
static bool five_step_mode = false;
static bool interrupt_inhibit = true;
static bool frame_interrupt_flag = false;

static bool pulse1_enabled = false;
static bool pulse2_enabled = false;
static bool triangle_enabled = false;
static bool noise_enabled = false;
static bool dmc_enabled = false;

void apu_write(uint16_t address, uint8_t value) {
	if (address >= 0x4000 && address <= 0x4003) {
		pulse_write_reg(&pulse1, address & 0b11, value);
	} else if (address >= 0x4004 && address <= 0x4007) {
		pulse_write_reg(&pulse2, address & 0b11, value);
	} else if (address >= 0x4008 && address <= 0x400B) {
		triangle_write_reg(address & 0b11, value);
	} else if(address >= 0x400C && address <= 0x400F) {
		noise_write_reg(address & 0b11, value);
	} else if (address == 0x4015) {
		// Status
		pulse1_enabled = value & 1;
		pulse2_enabled = value & 2;
		triangle_enabled = value & 4;
		noise_enabled = value & 8;
		dmc_enabled = value & 16;

		if (!pulse1_enabled) pulse1.lengthcounter.value = 0;
		if (!pulse2_enabled) pulse2.lengthcounter.value = 0;
		if (!triangle_enabled) triangle.lengthcounter.value = 0;
		if (!noise_enabled) noise.lengthcounter.value = 0;

	} else if (address == 0x4017) {
		five_step_mode = value & 0b10000000;
		interrupt_inhibit = value = 0b01000000;
		//apu_cycle_counter = 0; // Reset frame counter
		if (interrupt_inhibit) {
			frame_interrupt_flag = false;
		}

	}
}

uint8_t apu_read(uint16_t address) {
	if (address == 0x4015) {
		uint8_t value =
			(interrupt_inhibit ? 0x80 : 0)
			| (frame_interrupt_flag ? 0x40 : 0)
			| (noise.lengthcounter.value > 0 ? 8 : 0)
			| (triangle.lengthcounter.value > 0 ? 4 : 0)
			| (pulse2.lengthcounter.value > 0 ? 2 : 0)
			| (pulse1.lengthcounter.value > 0 ? 1 : 0);

		frame_interrupt_flag = false;
		return value;
	}

	return 0;
}

static void clock_linear_counters() {
	if (triangle.linear_counter_reload_flag) {
		triangle.linear_counter = triangle.linear_counter_reload;
	} else if (triangle.linear_counter > 0) {
		triangle.linear_counter--;
	}

	if (!triangle.lengthcounter.halt) {
		triangle.linear_counter_reload_flag = false;
	}
}

static void clock_length_counters_and_sweep_units() {
	clock_length_counter(&pulse1.lengthcounter);
	clock_length_counter(&pulse2.lengthcounter);
	clock_length_counter(&triangle.lengthcounter);
	clock_length_counter(&noise.lengthcounter);

	clock_sweep_unit(&pulse1);
	clock_sweep_unit(&pulse2);
}

static void clock_envelopes() {
	clock_envelope(&pulse1.envelope, pulse1.lengthcounter.halt);
	clock_envelope(&pulse2.envelope, pulse2.lengthcounter.halt);
	clock_envelope(&noise.envelope, noise.lengthcounter.halt);
}

void apu_tick_triangle() {
	if (triangle_enabled) {
		triangle_tick();
	}
}

void apu_tick(uint16_t scanline) {
	switch (apu_cycle_counter) {
		case 3728:
			clock_envelopes();
			clock_linear_counters();
			break;
		case 7456:
			clock_envelopes();
			clock_linear_counters();
			clock_length_counters_and_sweep_units();
			break;
		case 11185:
			clock_envelopes();
			clock_linear_counters();
			break;
		case 14914:
			if (!five_step_mode) {
				if (!interrupt_inhibit) {
					frame_interrupt_flag = true;
					irq6502();
				}
				clock_envelopes();
				clock_linear_counters();
				clock_length_counters_and_sweep_units();
			}
			break;
		case 14915:
			if (!five_step_mode) {
				apu_cycle_counter = 0;
			}
			break;
		case 18640:
			if (five_step_mode) {
				clock_envelopes();
				clock_linear_counters();
				clock_length_counters_and_sweep_units();
			}
			break;
		case 18641:
			if (five_step_mode) {
				apu_cycle_counter = 0;
			}
			break;
	}

	if (pulse1_enabled) {
		pulse_tick(&pulse1, true);
	}
	if (pulse2_enabled) {
		pulse_tick(&pulse2, false);
	}
	if (noise_enabled) {
		noise_tick();
	}

	int num_enabled = 4;
	int16_t frame_sample = 0;

	if (pulse1_enabled) {
		frame_sample += (int16_t)(((int)pulse1.current_output * 666) - 5000);
		//num_enabled++;
	}
	if (pulse2_enabled) {
		frame_sample += (int16_t)(((int)pulse2.current_output * 666) - 5000);
		//num_enabled++;
	}
	if (triangle_enabled) {
		frame_sample += (int16_t)(((int)triangle.current_output * 666) - 5000);
		//num_enabled++;
	}
	if (noise_enabled) {
		frame_sample += (int16_t)(((int)noise.current_output * 666) - 5000);
	}
	
	if (num_enabled > 0) {
		frame_sample /= num_enabled;
	}

	write_audio_sample(scanline, frame_sample);
	apu_cycle_counter++;
}

void apu_reset() {
	noise_reset();
}