cmake_minimum_required(VERSION 3.4)

if (WIN32)
	set(CMAKE_C_FLAGS "/Dinline=__inline /DWIN32 /DIN_MSX_PATH=C:\\Users\\user\\Documents\\msx\\wa2msx061213\\")
	set(CMAKE_EXE_LINKER_FLAGS "/DYNAMICBASE .\\Release\\kss.lib .\\modules\\emu2149\\Release\\emu2149.lib .\\modules\\emu2212\\Release\\emu2212.lib .\\modules\\emu2413\\Release\\emu2413.lib .\\modules\\emu8950\\Release\\emu8950.lib .\\modules\\emu76489\\Release\\emu76489.lib .\\modules\\kmz80\\Release\\kmz80.lib")

else()
	set(CMAKE_C_FLAGS "-O3 -Wall -DUSE_LIBAO -DIN_MSX_PATH=/home/zaffo/Documents/in_msx/")
endif()

if(NOT TARGET emu2149) 
	add_subdirectory(modules/emu2149) 
endif()

if(NOT TARGET emu2212)
	add_subdirectory(modules/emu2212)
endif()

if(NOT TARGET emu2413)
	add_subdirectory(modules/emu2413)
endif()

if(NOT TARGET emu8950)
	add_subdirectory(modules/emu8950)
endif()

if(NOT TARGET emu76489)
	add_subdirectory(modules/emu76489)
endif()

if(NOT TARGET kmz80)
	add_subdirectory(modules/kmz80)
endif()

include_directories(
	modules/drivers 
	modules/kmz80 
	modules/emu2149 
	modules/emu2212 
	modules/emu2413 
	modules/emu8950 
	modules/emu76489
)

file(GLOB SOURCES src/*.c src/**/*.c)

add_library(kss STATIC ${SOURCES})
target_link_libraries(kss emu2149 emu2212 emu2413 emu8950 emu76489 kmz80)

add_executable(kss2wav EXCLUDE_FROM_ALL src/kss2wav.c)
if (WIN32)
	target_link_libraries(kss2wav kss kernel32 winmm)
else()
	target_link_libraries(kss2wav kss m)
endif()

add_executable(sample_replay EXCLUDE_FROM_ALL src/sample_replay.c)
if (WIN32)
	target_link_libraries(sample_replay kss kernel32 winmm)
else()
	target_link_libraries(sample_replay kss ao m)
endif()
