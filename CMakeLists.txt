cmake_minimum_required(VERSION 3.13)
set(PSHELL pshell_uart)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(${PSHELL} C CXX ASM)
pico_sdk_init()
add_compile_options("-DPICO_MALLOC_PANIC=0 -DLFS_NO_ERROR -DLFS_NO_DEBUG -DLFS_NO_WARN")
add_subdirectory(littlefs)
add_subdirectory(stdinit-lib)
add_executable(${PSHELL} main.c xreceive.c xreceive.h xtransmit.c xtransmit.h
    crc16.c crc16.h fs.c fs.h vi.c vi.h cc.c cc.h cc_printf.S version.h)
pico_enable_stdio_uart(${PSHELL} 1)
pico_enable_stdio_usb(${PSHELL} 0)
target_link_libraries(${PSHELL} PUBLIC pico_stdlib littlefs stdinit-lib
    hardware_flash hardware_sync hardware_watchdog hardware_timer hardware_gpio)
pico_add_extra_outputs(${PSHELL})


