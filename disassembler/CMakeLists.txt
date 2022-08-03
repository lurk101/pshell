add_library(disassembler INTERFACE)
target_include_directories(disassembler INTERFACE ${CMAKE_CURRENT_LIST_DIR}/module)
target_sources(disassembler INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/module/armdisasm.c
    ${CMAKE_CURRENT_LIST_DIR}/module/armdisasm.h)

