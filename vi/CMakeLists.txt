add_library(vi INTERFACE)
target_include_directories(vi INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(vi INTERFACE
    vi.c vi.h
)
