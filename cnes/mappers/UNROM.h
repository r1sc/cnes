#ifndef _UNROM_H_
#define _UNROM_H_

#include <stdint.h>
#include "../nes001.h"
#include "../bit.h"

void unrom_reset();
uint8_t unrom_ppuRead(uint16_t address);
void unrom_ppuWrite(uint16_t address, uint8_t value);
uint8_t unrom_cpuRead(uint16_t address);
void unrom_cpuWrite(uint16_t address, uint8_t value);
void unrom_save_state(void* stream, stream_writer write);
void unrom_load_state(void* stream, stream_reader read);

#endif