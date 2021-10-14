cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(lfs C CXX ASM)
pico_sdk_init()
add_subdirectory(littlefs)
add_executable(lfs
    lfs.c
    xreceive.c xreceive.h
	xtransmit.c xtransmit.h
    crc16.c crc16.h
    pico_hal.c pico_hal.h
	vi.c vi.h)
pico_enable_stdio_uart(lfs 1)
pico_enable_stdio_usb(lfs 1)
target_link_libraries(lfs PUBLIC pico_stdlib littlefs hardware_flash hardware_sync)
pico_add_extra_outputs(lfs)
