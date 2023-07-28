#ifndef _MMC1_H_
#define _MMC1_H_

#include <stdint.h>
#include <assert.h>
#include "../nes001.h"
#include "../bit.h"

void mmc1_reset();
uint8_t mmc1_ppuRead(uint16_t address);
void mmc1_ppuWrite(uint16_t address, uint8_t value);
uint8_t mmc1_cpuRead(uint16_t address);
void mmc1_cpuWrite(uint16_t address, uint8_t value);
void mmc1_save_state(void* stream, stream_writer write);
void mmc1_load_state(void* stream, stream_reader read);


#endif