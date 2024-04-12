cmake_minimum_required(VERSION 3.8)
set (CMAKE_CXX_STANDARD 20)

project(cnes LANGUAGES C CXX)

add_library (cnes STATIC
	"fake6502.c"	
	"ppu.c"
	"disasm.c" 
	"nes001.c" 
	"apu.c" 
 "ppu.h" "mappers/MMC1.c" "mappers/NROM.c" "mappers/UNROM.c" "stream.h" "apu.cpp")

target_include_directories(cnes PUBLIC include)