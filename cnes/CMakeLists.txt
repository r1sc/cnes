cmake_minimum_required(VERSION 3.8)

project(cnes LANGUAGES C)

add_library (cnes STATIC
	"fake6502.c"	
	"ppu.c"
	"disasm.c" 
	"nes001.c" 
	"apu.c" 
	"ppu.h" 
	"mappers/MMC1.c" 
	"mappers/NROM.c" 
	"mappers/UNROM.c" 
	"stream.h" 
	"mappers/MMC2.h" 
	"mappers/MMC2.c"    "mappers/ColorDreams.h" "mappers/ColorDreams.c")

target_include_directories(cnes PUBLIC include)