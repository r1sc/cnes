#ifndef _NROM_H_
#define _NROM_H_

#include <assert.h>
#include <stdint.h>
#include "../nes001.h"
#include "../bit.h"
#include "../stream.h"

void nrom_reset();
uint8_t nrom_ppuRead(uint16_t address);
void nrom_ppuWrite(uint16_t address, uint8_t value);
uint8_t nrom_cpuRead(uint16_t address);
void nrom_cpuWrite(uint16_t address, uint8_t value);
void nrom_save_state(void* stream, stream_writer write);
void nrom_load_state(void* stream, stream_reader read);

#endif