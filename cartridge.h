#pragma once
#include <stdbool.h>
#include "ines.h"

extern ines_t ines;
extern uint8_t ciram[2048];

typedef uint8_t(*bus_read_t)(uint16_t address);
typedef void(*bus_write_t)(uint16_t address, uint8_t value);

extern bus_read_t cartridge_cpuRead;
extern bus_write_t cartridge_cpuWrite;
extern bus_read_t cartridge_ppuRead;
extern bus_write_t cartridge_ppuWrite;
