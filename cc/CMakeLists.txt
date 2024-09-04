add_library(cc INTERFACE)
if ("${PICO_BOARD}" STREQUAL "pico2")
set(CC_PROC "2350")
else()
set(CC_PROC "2040")
endif()
target_include_directories(cc INTERFACE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/${CC_PROC})
target_sources(cc INTERFACE
    cc.c cc.h cc_tokns.h cc_ops.h
    ${CC_PROC}/cc_defs.h
    ${CC_PROC}/cc_extrns.h
    cc_malloc.c cc_malloc.h
    cc_printf.S
)
