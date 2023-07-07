cmake_minimum_required(VERSION 3.13)

project(cnes_glfw)

add_subdirectory(deps/glfw-3.3.4)
#add_subdirectory(../agi ${CMAKE_CURRENT_BINARY_DIR}/tinyagi)

add_executable (cnes_glfw
	"fake6502.c" 
	"ines.c"
	"NROM.h" 
	"ppu.c"
	"window.c"
	"main.c" 
	"disasm.c")

	
if(MSVC)
  target_compile_options(cnes_glfw PUBLIC "/ZI")
  target_link_options(cnes_glfw PUBLIC "/INCREMENTAL")
endif()

	
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(OpenGL REQUIRED)

target_link_libraries(cnes_glfw glfw OpenGL::GL)