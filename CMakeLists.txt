cmake_minimum_required(VERSION 3.13)

option(USB_CONSOLE "build for USB console, otherwise UART" ON)
option(FORCE_TESTS "build release with tests cmd" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (USB_CONSOLE)
  set(PSHELL pshell_usb)
else()
  set(PSHELL pshell_uart)
endif()

project(${PSHELL} C CXX ASM)

pico_sdk_init()

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required.")
endif()

find_package(Git REQUIRED)
execute_process(
  COMMAND git describe
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE PSHELL_VERSION
)

add_compile_options("-Wno-stringop-overflow")
add_compile_options("-DPICO_MALLOC_PANIC=0")
add_compile_options("-DLFS_NO_ERROR")
add_compile_options("-DLFS_NO_DEBUG")
add_compile_options("-DLFS_NO_WARN")
add_compile_options("-DPSHELL_VERSION=${PSHELL_VERSION}")
if (FORCE_TESTS)
add_compile_options("-DPSHELL_TESTS")
endif()


add_subdirectory(littlefs)
add_subdirectory(disassembler)

add_executable(${PSHELL}
    src/main.c
    src/xmodem.c src/xmodem.h
    src/crc16.c src/crc16.h
    src/dgreadln.c src/dgreadln.h
    src/fs.c src/fs.h
    src/vi.c src/vi.h
    src/cc.c src/cc.h src/cc_extrns.h src/cc_tokns.h src/cc_ops.h src/cc_defs.h
    src/tar.c src/tar.h
    src/tests.c src/tests.h
    src/cc_printf.S
)

pico_set_linker_script(${PSHELL} ${CMAKE_CURRENT_LIST_DIR}/pshell.ld)

if (USB_CONSOLE)
  pico_enable_stdio_uart(${PSHELL} 0)
  pico_enable_stdio_usb(${PSHELL} 1)
else()
  pico_enable_stdio_uart(${PSHELL} 1)
  pico_enable_stdio_usb(${PSHELL} 0)
endif()

target_link_libraries(${PSHELL} PUBLIC
    littlefs disassembler
    pico_stdlib
    hardware_flash hardware_sync hardware_watchdog hardware_timer hardware_gpio
    hardware_pwm hardware_adc hardware_clocks hardware_uart hardware_i2c
    hardware_spi hardware_irq)

pico_add_extra_outputs(${PSHELL})

message("-----------------------------------------------------")
message("-- CMake ${CMAKE_VERSION}")
message("-- Build ${CMAKE_BUILD_TYPE} / ${CMAKE_SYSTEM_NAME}")
if (USB_CONSOLE)
message("-- USB_CONSOLE ${USB_CONSOLE}, using USB")
else()
message("-- USB_CONSOLE ${USB_CONSOLE}, using UART")
endif()
message("-----------------------------------------------------")

