#ifndef __COLORDREAMS_H_
#define __COLORDREAMS_H_

#include <stdint.h>
#include "../stream.h"
#include "../nes001.h"
#include "../bit.h"

void colordreams_reset();
uint8_t colordreams_ppuRead(uint16_t address);
void colordreams_ppuWrite(uint16_t address, uint8_t value);
uint8_t colordreams_cpuRead(uint16_t address);
void colordreams_cpuWrite(uint16_t address, uint8_t value);
void colordreams_save_state(void* stream, stream_writer write);
void colordreams_load_state(void* stream, stream_reader read);

#endif