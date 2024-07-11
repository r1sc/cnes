#include <cstdint>

#include "fake6502.h"

struct Timer {
	uint16_t current = 0;
	uint16_t reload = 0;

	bool tick() {
		if (current == 0) {
			current = reload;
			return true;
		}
		else {
			current--;
			return false;
		}
	}
};

struct LengthCounter {
	uint8_t value = 0;
	bool halt = false;

	const uint8_t length_table[32] = {
		10, 254, 20,  2, 40,  4, 80,  6,
		160,   8, 60, 10, 14, 12, 26, 14,
		12,  16, 24, 18, 48, 20, 96, 22,
		192,  24, 72, 26, 16, 28, 32, 30 };

	void load(uint8_t value) {
		value = length_table[value];
	}

	void clock() {
		if (!halt && value > 0) {
			value--;
		}
	}
};

struct Envelope {
	Timer timer = {};

	uint8_t decay_level = 0;
	bool start = false;
	bool constant_volume = false;

	void clock(bool loop_flag) {
		if (!start) {
			if (timer.tick()) {
				if (decay_level > 0) {
					decay_level--;
				}
				else if (loop_flag) {
					decay_level = 15;
				}
			}
		}
		else {
			start = false;
			decay_level = 15;
			timer.current = timer.reload;
		}
	}

	uint8_t get_volume() {
		return constant_volume ? (uint8_t)timer.reload : decay_level;
	}
};

struct Pulse {
	Timer timer = {};
	LengthCounter lengthcounter = {};
	Envelope envelope = {};

	uint8_t sequence = 0;
	uint8_t sequencer_pos = 0;
	uint8_t current_output = 0;

	bool sweep_enabled = false;
	uint16_t sweep_divider_current = 0;
	uint16_t sweep_divider_reload = 0;
	bool sweep_reload_flag = false;
	bool sweep_negate = false;
	uint8_t sweep_shift_count = 0;
	uint16_t sweep_target_period = 0;

	const uint8_t duty_cycles[4] = { 0b10000000, 0b11000000, 0b11110000, 0b00111111 };

	void calculate_sweep_target() {
		int change_amount = (int)(timer.reload >> sweep_shift_count);
		if (sweep_negate) {
			change_amount = -change_amount;
		}
		int target_period = (int)timer.reload + change_amount;
		if (target_period < 0) {
			target_period = 0;
		}
		sweep_target_period = (uint16_t)target_period;
	}

	void write_reg(uint8_t address, uint8_t value) {
		switch (address) {
		case 0:
		{
			uint8_t duty_cycle_index = (value >> 6) & 0b11;
			sequence = duty_cycles[duty_cycle_index];

			lengthcounter.halt = (value & 0b00100000) != 0;
			envelope.constant_volume = (value & 0b00010000) != 0;
			envelope.timer.reload = value & 0b1111;
		}
		break;
		case 1:
			sweep_enabled = value & 0x80;
			sweep_divider_reload = (value >> 4) & 0b111;
			sweep_negate = value & 0b1000;
			sweep_shift_count = value & 0b111;

			sweep_reload_flag = true;
			break;
		case 2:
			timer.reload = (timer.reload & 0x700) | value;
			//pulse_calculate_sweep_target(pulse);
			break;
		case 3:
			timer.reload = (timer.reload & 0xFF) | ((value & 0b111) << 8);
			timer.current = timer.reload;
			lengthcounter.load(value >> 3);
			sequencer_pos = 0; // Reset phase
			envelope.start = true;
			//pulse_calculate_sweep_target(pulse);
			break;
		}
	}

	void tick(bool sweep_ones_complement) {
		calculate_sweep_target();

		if (timer.tick()) {
			if (lengthcounter.value > 0 && timer.reload >= 8 && sweep_target_period <= 0x7FF) {
				current_output = ((sequence >> sequencer_pos) & 1) ? envelope.get_volume() : 0;
			}
			else {
				current_output = 0;
			}

			if (sequencer_pos == 0) {
				sequencer_pos = 7;
			}
			else {
				sequencer_pos--;
			}
		}
	}

	void clock_sweep_unit() {
		if (sweep_divider_current == 0 && sweep_enabled && timer.reload >= 8 && sweep_target_period <= 0x7FF) {
			timer.reload = sweep_target_period;
			//pulse_calculate_sweep_target(pulse);
		}
		if (sweep_divider_current == 0 || sweep_reload_flag) {
			sweep_divider_current = sweep_divider_reload;
			sweep_reload_flag = false;
		}
		else {
			sweep_divider_current--;
		}
	}
};

struct Triangle {
	Timer timer = {};
	LengthCounter lengthcounter = {};

	uint16_t linear_counter = 0;
	uint16_t linear_counter_reload = 0;
	bool linear_counter_reload_flag = false;
	uint8_t current_output = 0;
	uint8_t sequencer_pos = 0;

	const uint8_t triangle_sequence[32] = {
		15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
	};

	void write_reg(uint8_t address, uint8_t value) {
		switch (address) {
		case 0:
			linear_counter_reload = value & 0b01111111;
			lengthcounter.halt = (value & 0x80) == 0x80;
			break;
		case 2:
			timer.reload = (timer.reload & 0x700) | value;
			break;
		case 3:
			lengthcounter.load((value & 0xF8) >> 3);
			timer.reload = (timer.reload & 0xFF) | ((value & 0b111) << 8);
			timer.current = timer.reload;
			linear_counter_reload_flag = true;
			break;
		}
	}

	void tick() {
		if (timer.tick()) {
			if (linear_counter > 0 && lengthcounter.value > 0) {
				current_output = triangle_sequence[sequencer_pos];

				if (sequencer_pos == 31) {
					sequencer_pos = 0;
				}
				else {
					sequencer_pos++;
				}
			}
		}
	}
};

struct Noise {
	Timer timer = {};
	LengthCounter lengthcounter = {};
	Envelope envelope = {};

	uint8_t period_select = 0;
	uint8_t current_output = 0;
	uint16_t shift_reg = 1;
	bool mode = false;

	const uint16_t noise_period_table[16] = {
		4, 8, 16, 32, 64, 96, 128, 160,
		202, 254, 380, 508, 762, 1016, 2034, 4068
	};

	void tick() {
		if (timer.tick()) {
			uint16_t feedback = (shift_reg & 1) ^ ((mode ? (shift_reg >> 6) : (shift_reg >> 1)) & 1);
			shift_reg >>= 1;
			shift_reg = (shift_reg & 0x3FFF) | (feedback << 14);

			if ((shift_reg & 1) == 1 || lengthcounter.value == 0) {
				current_output = 0;
			}
			else {
				current_output = envelope.get_volume();
			}
		}
	}

	void write_reg(uint8_t address, uint8_t value) {
		switch (address) {
		case 0:
			lengthcounter.halt = value & 0b100000;
			envelope.constant_volume = value & 0b10000;
			envelope.timer.reload = value & 0b1111;
			break;
		case 2:
			timer.reload = noise_period_table[value & 0b1111];
			timer.current = timer.reload;
			break;
		case 3:
			lengthcounter.load(value >> 3);
			envelope.start = true;
			break;
		}
	}
};

extern "C" { void write_audio_sample(int scanline, int16_t sample); }

class APU {
	Pulse pulse1 = {};
	Pulse pulse2 = {};
	Triangle triangle = {};
	Noise noise = {};

	unsigned int apu_cycle_counter = 0;
	bool five_step_mode = false;
	bool interrupt_inhibit = true;
	bool frame_interrupt_flag = false;

	bool pulse1_enabled = false;
	bool pulse2_enabled = false;
	bool triangle_enabled = false;
	bool noise_enabled = false;
	bool dmc_enabled = false;

	void clock_linear_counters() {
		if (triangle.linear_counter_reload_flag) {
			triangle.linear_counter = triangle.linear_counter_reload;
		}
		else if (triangle.linear_counter > 0) {
			triangle.linear_counter--;
		}

		if (!triangle.lengthcounter.halt) {
			triangle.linear_counter_reload_flag = false;
		}
	}

	void clock_length_counters_and_sweep_units() {
		pulse1.lengthcounter.clock();
		pulse2.lengthcounter.clock();
		triangle.lengthcounter.clock();
		noise.lengthcounter.clock();

		pulse1.clock_sweep_unit();
		pulse2.clock_sweep_unit();
	}

	void clock_envelopes() {
		pulse1.envelope.clock(pulse1.lengthcounter.halt);
		pulse2.envelope.clock(pulse2.lengthcounter.halt);
		noise.envelope.clock(noise.lengthcounter.halt);
	}

public:
	void write(uint16_t address, uint8_t value) {
		if (address >= 0x4000 && address <= 0x4003) {
			pulse1.write_reg(address & 0b11, value);
		}
		else if (address >= 0x4004 && address <= 0x4007) {
			pulse2.write_reg(address & 0b11, value);
		}
		else if (address >= 0x4008 && address <= 0x400B) {
			triangle.write_reg(address & 0b11, value);
		}
		else if (address >= 0x400C && address <= 0x400F) {
			noise.write_reg(address & 0b11, value);
		}
		else if (address == 0x4015) {
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

		}
		else if (address == 0x4017) {
			five_step_mode = value & 0b10000000;
			interrupt_inhibit = value == 0b01000000;
			//apu_cycle_counter = 0; // Reset frame counter
			if (interrupt_inhibit) {
				frame_interrupt_flag = false;
			}
			if (five_step_mode) {
				clock_length_counters_and_sweep_units();
			}
		}
	}

	uint8_t read(uint16_t address) {
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

	void tick_triangle() {
		if (triangle_enabled) {
			triangle.tick();
		}
	}

	void tick(uint16_t scanline) {
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
			pulse1.tick(true);
		}
		if (pulse2_enabled) {
			pulse2.tick(false);
		}
		if (noise_enabled) {
			noise.tick();
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
			frame_sample += (int16_t)(((int)triangle.current_output * 533) - 4000);
			//num_enabled++;
		}
		if (noise_enabled) {
			frame_sample += (int16_t)(((int)noise.current_output * 333) - 2500);
		}

		if (num_enabled > 0) {
			frame_sample /= num_enabled;
		}

		write_audio_sample(scanline, frame_sample);
		apu_cycle_counter++;
	}
};