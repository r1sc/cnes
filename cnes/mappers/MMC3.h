#ifndef _MMC3_H_
#define _MMC3_H_

#include <stdint.h>
#include <assert.h>
#include "../nes001.h"
#include "../bit.h"

void mmc3_reset();
uint8_t mmc3_ppuRead(uint16_t address);
void mmc3_ppuWrite(uint16_t address, uint8_t value);
uint8_t mmc3_cpuRead(uint16_t address);
void mmc3_cpuWrite(uint16_t address, uint8_t value);
void mmc3_save_state(void* stream, stream_writer write);
void mmc3_load_state(void* stream, stream_reader read);
void mmc3_scanline();

#endif