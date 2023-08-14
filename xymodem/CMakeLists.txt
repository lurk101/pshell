add_library(xymodem INTERFACE)
target_include_directories(xymodem INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(xymodem INTERFACE
    xmodem.c xmodem.h
    ymodem.c ymodem.h
    crc16.c crc16.h
)
