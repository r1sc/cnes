#ifndef _MMC2_H_
#define _MMC2_H_

#include <stdint.h>
#include "../nes001.h"
#include "../bit.h"

void mmc2_reset();
uint8_t mmc2_ppuRead(uint16_t address);
void mmc2_ppuWrite(uint16_t address, uint8_t value);
uint8_t mmc2_cpuRead(uint16_t address);
void mmc2_cpuWrite(uint16_t address, uint8_t value);
void mmc2_save_state(void* stream, stream_writer write);
void mmc2_load_state(void* stream, stream_reader read);

#endif