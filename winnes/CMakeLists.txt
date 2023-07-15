cmake_minimum_required(VERSION 3.8)

project(winnes)

add_subdirectory(../cnes ${CMAKE_CURRENT_BINARY_DIR}/cnes)

add_executable (winnes WIN32
	"main.c"
	"window.c"
	"glstuff/glad.c"
)

if(MSVC)
  target_compile_options(winnes PUBLIC "/ZI")
  target_link_options(winnes PUBLIC "/INCREMENTAL")
endif()

target_link_libraries(winnes cnes opengl32.lib winmm.lib)