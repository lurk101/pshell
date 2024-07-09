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

static inline void dq_addfirst(dq_entry_t* node, dq_entry_t* queue) {
    node->prev = NULL;
    node->next = queue->next;

    if (!queue->next)
        queue->prev = node;
    else
        queue->next->prev = node;
    queue->next = node;
}

static inline void dq_rem(dq_entry_t* node, dq_entry_t* queue) {
    dq_entry_t* prev = node->prev;
    dq_entry_t* next = node->next;
    if (!prev)
        queue->next = next;
    else
        prev->next = next;
    if (!next)
        queue->prev = prev;
    else
        next->prev = prev;
    node->next = NULL;
    node->prev = NULL;
}

// local memory management functions
void* cc_malloc(int l, int die) {
    dq_entry_t* p = malloc(l + sizeof(dq_entry_t));
    if (!p) {
        if (die)
            run_fatal("out of memory");
        else
            return 0;
    }
    if (die)
        memset(p->data, 0, l);
    dq_addfirst(p, &malloc_list);
    return p->data;
}

void cc_free(void* p) {
    if (!p)
        run_fatal("freeing a NULL pointer");
    dq_entry_t* p2 = (dq_entry_t*)p - 1;
    dq_entry_t* pi = malloc_list.next;
    while (pi) {
        if (pi == p2) {
            dq_rem(p2, &malloc_list);
            free(pi);
            return;
        }
        pi = pi->next;
    }
    run_fatal("corrupted memory");
}

void cc_free_all(void) {
    while (!malloc_list.next)
        dq_rem(malloc_list.next, &malloc_list);
}
