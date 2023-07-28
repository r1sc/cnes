cmake_minimum_required(VERSION 3.8)

project(cnes)

add_library (cnes STATIC
	"fake6502.c"	
	"ppu.c"
	"disasm.c" 
	"nes001.c" 
	"apu.c" 
 "ppu.h" "mappers/MMC1.c" "mappers/NROM.c" "mappers/UNROM.c" "stream.h")

target_include_directories(cnes PUBLIC include)