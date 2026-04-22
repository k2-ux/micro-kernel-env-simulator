#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"

int cb_init(CircularBuffer *cb, int capacity)
{
    cb->data = (PacketView *)malloc((size_t)capacity * sizeof(PacketView));
    if (!cb->data) return -1;

    cb->base     = cb->data;
    cb->head     = cb->data;
    cb->tail     = cb->data;
    cb->count    = 0;
    cb->capacity = capacity;

    DBG("cb_init: %d slots at %p", capacity, (void *)cb->base);
    return 0;
}

void cb_free(CircularBuffer *cb)
{
    free(cb->data);
    cb->data = cb->base = cb->head = cb->tail = NULL;
    cb->count = cb->capacity = 0;
}

/* Advance pointer one slot, wrapping at the end — pure pointer arithmetic */
PacketView *cb_next(const CircularBuffer *cb, PacketView *p)
{
    PacketView *next = p + 1;
    if (next >= cb->base + cb->capacity)
        next = cb->base;
    return next;
}

void cb_push(CircularBuffer *cb, PacketView pv)
{
    *cb->head = pv;
    cb->head  = cb_next(cb, cb->head);

    if (cb->count < cb->capacity) {
        cb->count++;
    } else {
        /* Buffer full: discard oldest by advancing tail */
        cb->tail = cb_next(cb, cb->tail);
    }
}

/* Returns 0 on success, -1 when empty */
int cb_pop(CircularBuffer *cb, PacketView *out)
{
    if (cb->count == 0) return -1;
    *out     = *cb->tail;
    cb->tail = cb_next(cb, cb->tail);
    cb->count--;
    return 0;
}
