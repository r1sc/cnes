cmake_minimum_required(VERSION 3.8)

project(cnes)

add_library (cnes STATIC
	"fake6502.c"	
	"ppu.c"
	"disasm.c" 
	"nes001.c" 
	"apu.c" 
 "ppu.h")

if(MSVC)
  target_compile_options(cnes PUBLIC "/ZI")
  target_link_options(cnes PUBLIC "/INCREMENTAL")
endif()


target_include_directories(cnes PUBLIC include)