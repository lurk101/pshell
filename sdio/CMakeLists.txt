add_library(io INTERFACE)
target_include_directories(io INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(io INTERFACE
  sd_spi.c sd_spi.h
  io.c
)
target_link_libraries(io INTERFACE hardware_pio)
