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

static void timer_reset(timer_t* timer) {
	timer->current = 0;
	timer->reload = 0;
}

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

static void lengthcounter_reset(lengthcounter_t* length_counter) {
	length_counter->value = 0;
	length_counter->halt = false;
}

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

static void envelope_reset(envelope_t* envelope) {
	envelope->start = false;
	timer_reset(&envelope->timer);
	envelope->decay_level = 0;
	envelope->constant_volume = false;
}

static void clock_envelope(envelope_t* envelope, bool loop_flag) {
	if (!envelope->start) {
		if (timer_tick(&envelope->timer)) {
			if (envelope->decay_level > 0) {
				envelope->decay_level--;
			} else if (loop_flag) {
				envelope->decay_level = 15;
			}
		}
	} else {
		envelope->start = false;
		envelope->decay_level = 15;
		envelope->timer.current = envelope->timer.reload;
	}
}

static uint8_t envelope_get_volume(envelope_t* envelope) {
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

static void pulse_reset(apu_pulse_t* pulse) {
	timer_reset(&pulse->timer);
	lengthcounter_reset(&pulse->lengthcounter);
	pulse->sequence = 0;
	pulse->sequencer_pos = 0;
	pulse->current_output = 0;
	envelope_reset(&pulse->envelope);

	pulse->sweep_enabled = false;
	pulse->sweep_divider_current = 0;
	pulse->sweep_divider_reload = 0;
	pulse->sweep_reload_flag = false;
	pulse->sweep_negate = false;
	pulse->sweep_shift_count = 0;
	pulse->sweep_target_period = 0;
}

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

static void pulse_write_reg(apu_pulse_t* pulse, uint8_t address, uint8_t value, bool enabled) {
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
			if (enabled) {
				pulse->timer.reload = (pulse->timer.reload & 0xFF) | ((value & 0b111) << 8);
				pulse->timer.current = pulse->timer.reload;
				pulse->lengthcounter.value = length_table[value >> 3];
				pulse->sequencer_pos = 0; // Reset phase
				pulse->envelope.start = true;
			}
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

static void triangle_reset() {
	timer_reset(&triangle.timer);
	lengthcounter_reset(&triangle.lengthcounter);
	triangle.linear_counter = 0;
	triangle.linear_counter_reload = 0;
	triangle.linear_counter_reload_flag = false;
	triangle.current_output = 0;
	triangle.sequencer_pos = 0;
}

static void triangle_write_reg(uint8_t address, uint8_t value, bool enabled) {
	switch (address) {
		case 0:
			triangle.linear_counter_reload = value & 0b01111111;
			triangle.lengthcounter.halt = (value & 0x80) == 0x80;
			break;
		case 2:
			triangle.timer.reload = (triangle.timer.reload & 0x700) | value;
			break;
		case 3:
			if (enabled) {
				triangle.lengthcounter.value = length_table[(value & 0xF8) >> 3];
				triangle.timer.reload = (triangle.timer.reload & 0xFF) | ((value & 0b111) << 8);
				triangle.timer.current = triangle.timer.reload;
				triangle.linear_counter_reload_flag = true;
			}
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
	timer_reset(&noise.timer);
	lengthcounter_reset(&noise.lengthcounter);
	noise.period_select = 0;
	noise.current_output = 0;

	noise.shift_reg = 1;
	noise.mode = false;
	envelope_reset(&noise.envelope);
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

static void noise_write_reg(uint8_t address, uint8_t value, bool enabled) {
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
			if (enabled) {
				noise.lengthcounter.value = length_table[value >> 3];
				noise.envelope.start = true;
			}
			break;
	}
}

/******** DMC ************/

static struct {
	bool irq_enabled;
	bool loop;
	uint8_t output_level;

	timer_t timer;

	bool sample_buffer_filled;
	uint8_t sample_buffer;
	uint16_t sample_bytes_remaining;
	uint16_t current_address;
	uint16_t sample_address;
	uint16_t sample_length;

	uint8_t sr;
	uint8_t bits_remaining;
	bool silence;
	bool interrupt_flag;
} dmc;

static uint8_t dmc_rate_table[] = { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54 };

static void dmc_reset() {
	dmc.irq_enabled = false;
	dmc.loop = false;
	dmc.output_level = 0;

	timer_reset(&dmc.timer);

	dmc.sample_buffer_filled = false;
	dmc.sample_buffer = 0;
	dmc.sample_bytes_remaining = 0;
	dmc.current_address = 0;
	dmc.sample_address = 0;
	dmc.sample_length = 0;

	dmc.sr = 0;
	dmc.bits_remaining = 0;
	dmc.silence = true;
	dmc.interrupt_flag = false;
}

static void dmc_start_sample() {
	dmc.current_address = dmc.sample_address;
	dmc.sample_bytes_remaining = dmc.sample_length;
}

static void dmc_tick_memory_reader() {
	if (!dmc.sample_buffer_filled && dmc.sample_bytes_remaining > 0) {
		// Sample buffer is empty
		dmc.sample_buffer = read6502(dmc.current_address++);
		dmc.sample_buffer_filled = true;
		if (dmc.current_address == 0) { // wrapped around
			dmc.current_address = 0x8000;
		}
		dmc.sample_bytes_remaining--;
	}

	if (dmc.sample_bytes_remaining == 0) {
		if (dmc.loop) {
			dmc_start_sample();
		} else if (dmc.irq_enabled) {
			dmc.interrupt_flag = true;
		}
	}
}

static void dmc_tick() {
	dmc_tick_memory_reader();

	if (dmc.interrupt_flag) {
		irq6502();
	}

	if (timer_tick(&dmc.timer)) {
		if (!dmc.silence) {
			uint8_t b = dmc.sr & 1;
			if (b == 1 && dmc.output_level <= 125) {
				dmc.output_level += 2;
			} else if (b == 0 && dmc.output_level >= 2) {
				dmc.output_level -= 2;
			}
		}

		dmc.sr >>= 1;

		if (dmc.bits_remaining == 0) {
			dmc.bits_remaining = 8;
			if (!dmc.sample_buffer_filled) {
				dmc.silence = true;
				dmc.output_level = 0;
			} else {
				dmc.silence = false;
				dmc.sr = dmc.sample_buffer;
				dmc.sample_buffer_filled = false;
			}
		} else {
			dmc.bits_remaining--;
		}
	}
}

static void dmc_write_reg(uint8_t address, uint8_t value) {
	switch (address) {
		case 0:
			dmc.irq_enabled = (value & 0x80) == 0x80;
			dmc.loop = (value & 0x40) == 0x40;
			dmc.timer.reload = dmc_rate_table[value & 0x0F];
			dmc.timer.current = dmc.timer.reload;
			if (!dmc.irq_enabled) dmc.interrupt_flag = false;
			break;
		case 1:
			dmc.output_level = value & 0x7F;
			break;
		case 2:
			dmc.sample_address = 0xC000 + (value << 6);
			break;
		case 3:
			dmc.sample_length = (value << 4) + 1;
			break;
	}
}


/***** APU *****/

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

void apu_write(uint16_t address, uint8_t value) {
	if (address >= 0x4000 && address <= 0x4003) {
		pulse_write_reg(&pulse1, address & 0b11, value, pulse1_enabled);
	} else if (address >= 0x4004 && address <= 0x4007) {
		pulse_write_reg(&pulse2, address & 0b11, value, pulse2_enabled);
	} else if (address >= 0x4008 && address <= 0x400B) {
		triangle_write_reg(address & 0b11, value, triangle_enabled);
	} else if (address >= 0x400C && address <= 0x400F) {
		noise_write_reg(address & 0b11, value, noise_enabled);
	} else if (address >= 0x4010 && address <= 0x4013) {
		dmc_write_reg(address & 0b11, value);
	} else if (address == 0x4015) {
		// Status
		pulse1_enabled = (value & 1) == 1;
		pulse2_enabled = (value & 2) == 2;
		triangle_enabled = (value & 4) == 4;
		noise_enabled = (value & 8) == 8;
		dmc_enabled = (value & 16) == 16;

		dmc.interrupt_flag = false;

		if (!pulse1_enabled) pulse1.lengthcounter.value = 0;
		if (!pulse2_enabled) pulse2.lengthcounter.value = 0;
		if (!triangle_enabled) triangle.lengthcounter.value = 0;
		if (!noise_enabled) noise.lengthcounter.value = 0;
		if (!dmc_enabled) {
			dmc.sample_bytes_remaining = 0;
		} else {
			dmc_start_sample();
		}

		dmc_tick_memory_reader();

	} else if (address == 0x4017) {
		five_step_mode = value & 0b10000000;
		interrupt_inhibit = value = 0b01000000;
		//apu_cycle_counter = 0; // Reset frame counter
		if (interrupt_inhibit) {
			frame_interrupt_flag = false;
		}
		if (five_step_mode) {
			clock_length_counters_and_sweep_units();
		}
	}
}

uint8_t apu_read(uint16_t address) {
	if (address == 0x4015) {
		uint8_t value =
			(interrupt_inhibit ? 0x80 : 0)
			| (frame_interrupt_flag ? 0x40 : 0)
			| (dmc.sample_bytes_remaining > 0 ? 16 : 0)
			| (noise.lengthcounter.value > 0 ? 8 : 0)
			| (triangle.lengthcounter.value > 0 ? 4 : 0)
			| (pulse2.lengthcounter.value > 0 ? 2 : 0)
			| (pulse1.lengthcounter.value > 0 ? 1 : 0);

		frame_interrupt_flag = false;
		return value;
	}

	return 0;
}

void apu_tick_triangle() {
	if (triangle_enabled) {
		triangle_tick();
	}
	if (dmc_enabled) {
		dmc_tick();
	}
}

static int16_t pulse_lookup_table[31];
static int16_t tnd_lookup_table[203];

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

	int16_t pulse_out = pulse_lookup_table[(size_t)pulse1.current_output + (size_t)pulse2.current_output];
	int16_t tnd_out = tnd_lookup_table[3 * (size_t)triangle.current_output + 2 * (size_t)noise.current_output + (size_t)dmc.output_level];
	int16_t frame_sample = pulse_out + tnd_out;
	
	write_audio_sample(scanline, frame_sample);
	apu_cycle_counter++;
}

void apu_reset() {
	// Generate pulse lookup table
	for (size_t i = 0; i < 31; i++) {
		pulse_lookup_table[i] = (int16_t)((95.52 / (8128.0 / (double)i + 100)) * INT16_MAX);
	}
	// Generate triangle, noise and DMC lookup table
	for (size_t i = 0; i < 203; i++) {
		tnd_lookup_table[i] = (int16_t)((163.67 / (24329.0 / (double)i + 100)) * INT16_MAX);
	}

	triangle_reset();
	noise_reset();
	pulse_reset(&pulse1);
	pulse_reset(&pulse2);

	apu_cycle_counter = 0;
	five_step_mode = false;
	interrupt_inhibit = true;
	frame_interrupt_flag = false;

	pulse1_enabled = false;
	pulse2_enabled = false;
	triangle_enabled = false;
	noise_enabled = false;
	dmc_enabled = false;
}