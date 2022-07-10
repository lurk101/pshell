cmake_minimum_required(VERSION 3.13)

option(USB_CONSOLE "build for USB console" 0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (USB_CONSOLE)
  set(PSHELL pshell_usb)
else()
  set(PSHELL pshell_uart)
  message("Building for UART. For USB use -DUSB_CONSOLE=ON")
endif()

project(${PSHELL} C CXX ASM)

pico_sdk_init()

# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_options("-DPICO_MALLOC_PANIC=0")
add_compile_options("-DLFS_NO_ERROR")
add_compile_options("-DLFS_NO_DEBUG")
add_compile_options("-DLFS_NO_WARN")
add_compile_options("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")
add_compile_options("-DGIT_BRANCH=${GIT_BRANCH}")

add_subdirectory(littlefs)
add_subdirectory(stdinit-lib)

add_executable(${PSHELL}
    main.c
    xreceive.c xreceive.h
    xtransmit.c xtransmit.h
    crc16.c crc16.h
    fs.c fs.h
    vi.c vi.h
    cc.c cc.h
    io.c io.h
    cc_printf.S
    version.h)

if (USB_CONSOLE)
  pico_enable_stdio_uart(${PSHELL} 0)
  pico_enable_stdio_usb(${PSHELL} 1)
else()
  pico_enable_stdio_uart(${PSHELL} 1)
  pico_enable_stdio_usb(${PSHELL} 0)
endif()

set(LINK_LIST)
list(APPEND LINK_LIST
    pico_stdlib
    littlefs stdinit-lib
    hardware_flash hardware_sync hardware_watchdog hardware_timer hardware_gpio
    hardware_pwm hardware_adc hardware_clocks hardware_uart hardware_i2c
    hardware_spi hardware_irq)

if (NOT USB_CONSOLE)
    list(APPEND LINK_LIST hardware_uart hardware_irq)
endif()

target_link_libraries(${PSHELL} PUBLIC ${LINK_LIST})

pico_add_extra_outputs(${PSHELL})

