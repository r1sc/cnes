cmake_minimum_required(VERSION 3.8)

project(cnes_win32)

add_executable (cnes_win32 WIN32 
	"fake6502.c" 
	"ines.c"
	"NROM.h" 
	"ppu.c"
	"window.c"
	"main.c" 
	"disasm.c" "UNROM.h" "MMC1.h" "nes001.c" "nes001.h" "glstuff/glad.c")
		
if(MSVC)
  target_compile_options(cnes_win32 PUBLIC "/Zi")
  target_link_options(cnes_win32 PUBLIC "/INCREMENTAL")
endif()

target_link_libraries(cnes_win32 opengl32.lib  	Dwmapi.lib)