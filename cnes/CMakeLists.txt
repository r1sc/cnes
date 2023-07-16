cmake_minimum_required(VERSION 3.8)

project(cnes)

add_library (cnes STATIC
	"fake6502.c"	
	"ppu.c"
	"disasm.c" 
	"nes001.c" 
	"apu.c" 
)

target_include_directories(cnes PUBLIC include)