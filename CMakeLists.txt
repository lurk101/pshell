cmake_minimum_required(VERSION 3.13)

option(USB_CONSOLE "build for USB console, otherwise UART" ON)
option(FORCE_TESTS "build release with tests cmd" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(PSHELL pshell)

project(${PSHELL} C CXX ASM)

pico_sdk_init()

find_package(Git REQUIRED)
execute_process(
    COMMAND git describe
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE PSHELL_GIT_TAG
)

add_subdirectory(littlefs)
add_subdirectory(disassembler)
add_subdirectory(vi)
add_subdirectory(cc)
add_subdirectory(misc)
add_subdirectory(xymodem)

if ("${PICO_BOARD}" STREQUAL "vgaboard")
    add_subdirectory(sdio)
else()
    add_subdirectory(flashio)
endif()

add_executable(${PSHELL}
    shell.c
)

target_compile_definitions(${PSHELL} PUBLIC
    PICO_MALLOC_PANIC=0
    PSHELL_GIT_TAG=\"${PSHELL_GIT_TAG}\"
)

if (FORCE_TESTS)
    target_compile_definitions(${PSHELL} PUBLIC PSHELL_TESTS)
endif()

if ("${PICO_BOARD}" STREQUAL "pico2")
    pico_set_linker_script(${PSHELL} ${CMAKE_CURRENT_LIST_DIR}/misc/pshell_2350.ld)
else()
    pico_set_linker_script(${PSHELL} ${CMAKE_CURRENT_LIST_DIR}/misc/pshell_2040.ld)
endif()

if (USB_CONSOLE)
    pico_enable_stdio_uart(${PSHELL} 0)
    pico_enable_stdio_usb(${PSHELL} 1)
else()
    pico_enable_stdio_uart(${PSHELL} 1)
    pico_enable_stdio_usb(${PSHELL} 0)
endif()

pico_set_program_name(${PSHELL} "pshell")
pico_set_program_version(${PSHELL} "${PSHELL_GIT_TAG}")

target_link_libraries(${PSHELL} PUBLIC
    littlefs disassembler io vi cc misc xymodem
    pico_stdlib pico_rand
    hardware_flash hardware_sync hardware_watchdog hardware_timer hardware_gpio
    hardware_pwm hardware_adc hardware_clocks hardware_uart hardware_i2c
    hardware_spi hardware_irq hardware_dma hardware_uart)

pico_add_extra_outputs(${PSHELL})

message("-----------------------------------------------------")
message("-- CMake ${CMAKE_VERSION}")
message("-- Build ${CMAKE_BUILD_TYPE} / ${CMAKE_SYSTEM_NAME}")
if (USB_CONSOLE)
message("-- USB_CONSOLE ${USB_CONSOLE}, using USB")
else()
message("-- USB_CONSOLE ${USB_CONSOLE}, using UART")
endif()
if ("${PICO_BOARD}" STREQUAL "vgaboard")
message("-- building for ${PICO_BOARD}, using SD card file system")
else()
message("-- building for ${PICO_BOARD}, using flash file system")
endif()
message("-----------------------------------------------------")
