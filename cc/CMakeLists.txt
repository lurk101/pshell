add_library(cc INTERFACE)
target_include_directories(cc INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_sources(cc INTERFACE
    cc.c cc.h cc_tokns.h cc_ops.h
    cc_defs.h cc_extrns.h
    cc_malloc.c cc_malloc.h
    cc_peep.c cc_peep.h
    cc_printf.S 
)
