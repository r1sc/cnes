cmake_minimum_required(VERSION 3.8)

project(winnes)

add_subdirectory(../cnes ${CMAKE_CURRENT_BINARY_DIR}/cnes)

add_executable (winnes WIN32
	"main.c"
	"window.c"
	"glstuff/glad.c"
 "joystick.c"
 "waveout.c"
"winnes.rc"
)

set_source_files_properties(winres.rc PROPERTIES LANGUAGE RC)

target_link_libraries(winnes cnes opengl32.lib winmm.lib Xinput.lib)