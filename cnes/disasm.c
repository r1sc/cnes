#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fake6502.h"

uint16_t disassembler_offset;

static void read8() {
	uint8_t value = read6502(disassembler_offset++);
	printf("$%02X", value);
}

static void read16() {
	uint16_t lo = (uint16_t)read6502(disassembler_offset++);
	uint16_t hi = (uint16_t)read6502(disassembler_offset++);
	printf("$%04X", (hi << 8) | lo);
}

static void zeropage_x() {
	printf("("); read8(); printf(",X)");
}

static void zeropage() {
	read8();
}

static void immediate() {
	printf("#"); read8();
}

static void absolute() {
	read16();
}

static void indirect() {
	printf("("); read16(); printf(")");
}

static void zeropage_y() {
	printf("("); read8(); printf("),Y");
}

static void zeropage_x2() {
	read8(); printf(",X");
}

static void absolute_y() {
	read16(); printf(",Y");
}

static void absolute_x() {
	read16(); printf(",X");
}

static void accumulator() {
	printf("A");
}

static void relative() {
	int d = (int)read6502(disassembler_offset++);
	if (d & 0x80) {
		d = -(d ^ 0xFF);
	}
	printf("%d", d);
}

void disassemble() {
	uint8_t opcode = read6502(disassembler_offset++);
	uint8_t aaa = opcode >> 5;
	uint8_t bbb = (opcode >> 2) & 7;
	uint8_t cc = opcode & 3;

	switch (opcode) {
		case 0x00: printf("BRK"); break;
		case 0x08: printf("PHP"); break;
		case 0x10: printf("BPL "); relative(); break;
		case 0x18: printf("CLC"); break;
		case 0x20: printf("JSR "); absolute(); break;
		case 0x28: printf("PLP"); break;
		case 0x30: printf("BMI "); relative(); break;
		case 0x38: printf("SEC"); break;
		case 0x40: printf("RTI"); break;
		case 0x48: printf("PHA"); break;
		case 0x50: printf("BVC "); relative(); break;
		case 0x58: printf("CLI"); break;
		case 0x60: printf("RTS"); break;
		case 0x68: printf("PLA"); break;
		case 0x70: printf("BVS "); relative(); break;
		case 0x78: printf("SEI"); break;
		case 0x88: printf("DEY"); break;
		case 0x8A: printf("TXA"); break;
		case 0x90: printf("BCC "); relative(); break;
		case 0x98: printf("TYA"); break;
		case 0x9A: printf("TXS"); break;
		case 0xA8: printf("TAY"); break;
		case 0xAA: printf("TAX"); break;
		case 0xB0: printf("BCS "); relative(); break;
		case 0xB8: printf("CLV"); break;
		case 0xBA: printf("TSX"); break;
		case 0xC8: printf("INY"); break;
		case 0xCA: printf("DEX"); break;
		case 0xD0: printf("BNE "); relative(); break;
		case 0xD8: printf("CLD"); break;
		case 0xE8: printf("INX"); break;
		case 0xEA: printf("NOP"); break;
		case 0xF0: printf("BEQ "); relative(); break;
		case 0xF8: printf("SED"); break;
		default:
			switch (cc) {
				case 0:
					switch (aaa) {
						case 1: printf("BIT "); break;
						case 2: printf("JMP "); break;
						case 3: printf("JMP "); break;
						case 4: printf("STY "); break;
						case 5: printf("LDY "); break;
						case 6: printf("CPY "); break;
						case 7: printf("CPX "); break;
					}
					switch (bbb) {
						case 0: immediate(); break;
						case 1: zeropage(); break;
						case 3: aaa == 3 ? indirect() : absolute(); break;
						case 5: zeropage_x2(); break;
						case 7: absolute_x(); break;
					}
					break;
				case 1:
					switch (aaa) {
						case 0: printf("ORA "); break;
						case 1: printf("AND "); break;
						case 2: printf("EOR "); break;
						case 3: printf("ADC "); break;
						case 4: printf("STA "); break;
						case 5: printf("LDA "); break;
						case 6: printf("CMP "); break;
						case 7: printf("SBC "); break;
					}
					switch (bbb) {
						case 0: zeropage_x(); break;
						case 1: zeropage(); break;
						case 2: immediate(); break;
						case 3: absolute(); break;
						case 4: zeropage_y(); break;
						case 5: zeropage_x2(); break;
						case 6: absolute_y(); break;
						case 7: absolute_x(); break;
					}
					break;
				case 2:
					switch (aaa) {
						case 0: printf("ASL "); break;
						case 1: printf("ROL"); break;
						case 2: printf("LSR "); break;
						case 3: printf("ROR "); break;
						case 4: printf("STX "); break;
						case 5: printf("LDX "); break;
						case 6: printf("DEC "); break;
						case 7: printf("INC "); break;
					}
					switch (bbb) {
						case 0: immediate(); break;
						case 1: zeropage(); break;
						case 2: accumulator(); break;
						case 3: absolute(); break;
						case 5: aaa == 4 || aaa == 5 ? zeropage_y() : zeropage_x2(); break;
						case 7: aaa == 5 ? absolute_y() : absolute_x(); break;
					}
					break;
				default:
					break;
			}
	}
	printf("\n");
}