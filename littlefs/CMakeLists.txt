add_library(littlefs INTERFACE)
target_include_directories(littlefs INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/module
)
target_sources(littlefs INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/module/lfs.c
  ${CMAKE_CURRENT_LIST_DIR}/module/lfs.h
  ${CMAKE_CURRENT_LIST_DIR}/module/lfs_util.c
  ${CMAKE_CURRENT_LIST_DIR}/module/lfs_util.h
)
target_compile_definitions(littlefs INTERFACE
  LFS_NO_ERROR
  LFS_NO_DEBUG
  LFS_NO_WARN
)


