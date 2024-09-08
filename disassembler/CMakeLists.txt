add_library(disassembler INTERFACE)
target_include_directories(disassembler INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(disassembler INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/armdisasm.c
    ${CMAKE_CURRENT_LIST_DIR}/armdisasm.h)
