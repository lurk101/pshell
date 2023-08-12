#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#include "cc_malloc.h"

#define UDATA __attribute__((section(".ccudata")))

__attribute__((__noreturn__)) void run_fatal(const char* fmt, ...);

static int* malloc_list UDATA; // list of allocated memory blocks

// local memory management functions
void* cc_malloc(int l, int die) {
    int* p = malloc(l + 4);
    if (!p) {
        if (die)
            run_fatal("out of memory");
        else
            return 0;
    }
    if (die)
        memset(p + 1, 0, l);
    p[0] = (int)malloc_list;
    malloc_list = p;
    return p + 1;
}

void cc_free(void* p) {
    if (!p)
        run_fatal("freeing a NULL pointer");
    int* p2 = (int*)p - 1;
    int* last = (int*)&malloc_list;
    int* pi = (int*)(*last);
    while (pi) {
        if (pi == p2) {
            last[0] = pi[0];
            free(pi);
            return;
        }
        last = pi;
        pi = (int*)pi[0];
    }
    run_fatal("corrupted memory");
}

void cc_free_all(void) {
    while (malloc_list)
        cc_free(malloc_list + 1);
}
