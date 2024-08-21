add_library(cc INTERFACE)
if ("${PICO_BOARD}" STREQUAL "pico2")
target_include_directories(cc INTERFACE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/2350)
target_sources(cc INTERFACE
    cc.c cc.h cc_extrns.h cc_tokns.h cc_ops.h
    2350/cc_defs.h
    cc_malloc.c cc_malloc.h
    cc_printf.S
)
else()
target_include_directories(cc INTERFACE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/2040)
target_sources(cc INTERFACE
    cc.c cc.h cc_extrns.h cc_tokns.h cc_ops.h
    2040/cc_defs.h
    cc_malloc.c cc_malloc.h
    cc_printf.S
)
endif()
