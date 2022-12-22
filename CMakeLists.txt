cmake_minimum_required(VERSION 3.13)

option(USB_CONSOLE "build for USB console, otherwise UART" ON)
option(FORCE_TESTS "build release with tests cmd" OFF)
option(SD_IO "build for SD card instead of built-in flash" OFF)
option(USE_UART1 "use UART 1" OFF)

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
  OUTPUT_VARIABLE PSHELL_GIT_TAG
)

add_compile_options("-DPICO_MALLOC_PANIC=0")
add_compile_options("-DLFS_NO_ERROR")
add_compile_options("-DLFS_NO_DEBUG")
add_compile_options("-DLFS_NO_WARN")
add_compile_options("-DPSHELL_GIT_TAG=${PSHELL_GIT_TAG}")
if (FORCE_TESTS)
add_compile_options("-DPSHELL_TESTS")
endif()
if (USE_UART1)
  add_compile_options("-DPICO_DEFAULT_UART=1")
  add_compile_options("-DPICO_DEFAULT_UART_TX_PIN=20")
  add_compile_options("-DPICO_DEFAULT_UART_RX_PIN=21")
endif()


add_subdirectory(littlefs)
add_subdirectory(disassembler)
if (SD_IO)
  add_subdirectory(sdio)
else()
  add_subdirectory(flashio)
endif()


add_executable(${PSHELL}
    src/main.c
    src/xmodem.c src/xmodem.h
    src/crc16.c src/crc16.h
    src/readln.c src/readln.h
    src/vi.c src/vi.h
    src/cc.c src/cc.h src/cc_extrns.h src/cc_tokns.h src/cc_ops.h src/cc_defs.h
    src/tar.c src/tar.h
    src/tests.c src/tests.h
    src/cc_printf.S
)

target_include_directories(${PSHELL} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/${FS_DIR})

pico_set_linker_script(${PSHELL} ${CMAKE_CURRENT_LIST_DIR}/src/pshell.ld)

if (USB_CONSOLE)
  pico_enable_stdio_uart(${PSHELL} 0)
  pico_enable_stdio_usb(${PSHELL} 1)
else()
  pico_enable_stdio_uart(${PSHELL} 1)
  pico_enable_stdio_usb(${PSHELL} 0)
endif()

target_link_libraries(${PSHELL} PUBLIC
    littlefs disassembler io
    pico_stdlib
    hardware_flash hardware_sync hardware_watchdog hardware_timer hardware_gpio
    hardware_pwm hardware_adc hardware_clocks hardware_uart hardware_i2c
    hardware_spi hardware_irq hardware_dma)

pico_add_extra_outputs(${PSHELL})

message("-----------------------------------------------------")
message("-- CMake ${CMAKE_VERSION}")
message("-- Build ${CMAKE_BUILD_TYPE} / ${CMAKE_SYSTEM_NAME}")
if (USB_CONSOLE)
message("-- USB_CONSOLE ${USB_CONSOLE}, using USB")
else()
message("-- USB_CONSOLE ${USB_CONSOLE}, using UART")
endif()
if (SD_IO)
message("-- SD_IO ${SD_IO}, using SD card file system")
else()
message("-- SD_IO ${SD_IO}, using internal flash file system")
endif()
if (USE_UART1)
message("-- USE_UART1 ${USE_UART1}, using UART1")
else()
message("-- USE_UART1 ${USE_UART1}, using UART0")
endif()
message("-----------------------------------------------------")

