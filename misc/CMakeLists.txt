add_library(misc INTERFACE)
target_include_directories(misc INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(misc INTERFACE
    xmodem.c xmodem.h
    crc16.c crc16.h
    readln.c readln.h
    tar.c tar.h
    tests.c tests.h

)
