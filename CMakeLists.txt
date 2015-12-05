cmake_minimum_required(VERSION 3.4)
add_definitions("-DEMBED_MGSDRV=1")

set(CMAKE_C_FLAGS "-O3 -Wall")

add_subdirectory(modules/emu2149)
add_subdirectory(modules/emu2212)
add_subdirectory(modules/emu2413)
add_subdirectory(modules/emu8950)
add_subdirectory(modules/emu76489)
add_subdirectory(modules/kmz80)

include_directories(modules/kmz80 modules/emu2149 modules/emu2212 modules/emu2413 modules/emu8950 modules/emu76489)

file(GLOB SOURCES src/*.c src/**/*.c)
set(CMAKE_C_FLAGS "-Wall")

add_library(kss STATIC ${SOURCES})
target_link_libraries(kss emu2149 emu2212 emu2413 emu8950 emu76489 kmz80)

add_executable(kss2wav src/kss2wav.c)
target_link_libraries(kss2wav kss)

if(EMSCRIPTEN)
  set (CMAKE_EXE_LINKER_FLAGS "-O3")
endif()

