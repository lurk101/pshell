add_library(io INTERFACE)
target_include_directories(io INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(io INTERFACE
  crc.c crc.h
  hw_config.c hw_config.h
  sd_card.c sd_card.h
  sd_spi.c sd_spi.h
  spi.c spi.h
  fs.c fs.h
)
