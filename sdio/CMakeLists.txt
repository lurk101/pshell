add_library(io INTERFACE)
target_include_directories(io INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(io INTERFACE
  sd_spi.c sd_spi.h
  fs.c fs.h
  sd_spi.pio
)
pico_generate_pio_header(io ${CMAKE_CURRENT_LIST_DIR}/sd_spi.pio)
