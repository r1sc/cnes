cmake_minimum_required(VERSION 3.8)
set (CMAKE_CXX_STANDARD 20)

project(winnes)

add_subdirectory(../cnes ${CMAKE_CURRENT_BINARY_DIR}/cnes)

add_executable (winnes WIN32
	"main.cpp"
	"window.cpp"
	"glstuff/glad.c"
 "joystick.cpp"
 "waveout.c"
"winnes.rc"
 "joystick.h")

set_source_files_properties(winres.rc PROPERTIES LANGUAGE RC)

target_link_libraries(winnes cnes opengl32.lib winmm.lib Xinput.lib)