add_library(misc INTERFACE)
target_include_directories(misc INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(misc INTERFACE
    readln.c readln.h
    tar.c tar.h
    tests.c tests.h

)
