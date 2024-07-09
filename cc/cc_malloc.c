#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#include "cc_malloc.h"

typedef struct dq_entry_s {
    struct dq_entry_s* next;
    struct dq_entry_s* prev;
    char data[0];
} dq_entry_t;

#define UDATA __attribute__((section(".ccudata")))

__attribute__((__noreturn__)) void run_fatal(const char* fmt, ...);

static dq_entry_t malloc_list UDATA; // list of allocated memory blocks

static inline void dq_addfirst(dq_entry_t* node) {
    node->prev = NULL;
    node->next = malloc_list.next;

    if (!malloc_list.next)
        malloc_list.prev = node;
    else
        malloc_list.next->prev = node;
    malloc_list.next = node;
}

static inline void dq_rem(dq_entry_t* node) {
    dq_entry_t* prev = node->prev;
    dq_entry_t* next = node->next;
    if (!prev)
        malloc_list.next = next;
    else
        prev->next = next;
    if (!next)
        malloc_list.prev = prev;
    else
        next->prev = prev;
}

static inline dq_entry_t* dq_find(void* addr) {
    dq_entry_t* p = (dq_entry_t*)addr - 1;
    for (dq_entry_t* p2 = malloc_list.next; p2; p2 = p2->next)
        if (p2 == p)
            return p2;
    return NULL;
}

// local memory management functions
void* cc_malloc(int l, int cc, int zero) {
    dq_entry_t* p = malloc(l + sizeof(dq_entry_t));
    if (!p) {
        if (cc)
            run_fatal("out of memory");
        else
            return 0;
    }
    if (zero)
        memset(p->data, 0, l);
    dq_addfirst(p);
    return p->data;
}

void cc_free(void* p) {
    if (!p)
        run_fatal("freeing a NULL pointer");
    dq_entry_t* p2 = dq_find(p);
    if (p2) {
        dq_rem(p2);
        free(p2);
        return;
    }
    run_fatal("corrupted memory");
}

void cc_free_all(void) {
    while (!malloc_list.next)
        dq_rem(malloc_list.next);
}
