#ifndef BUFFER_H
#define BUFFER_H

#include "sensor.h"

typedef struct {
    PacketView *data;      /* malloc'd array                    */
    PacketView *base;      /* first element (arithmetic anchor) */
    PacketView *head;      /* next write position               */
    PacketView *tail;      /* oldest unread entry               */
    int         count;
    int         capacity;
} CircularBuffer;

int         cb_init(CircularBuffer *cb, int capacity);
void        cb_free(CircularBuffer *cb);
void        cb_push(CircularBuffer *cb, PacketView pv);
int         cb_pop(CircularBuffer *cb, PacketView *out);
PacketView *cb_next(const CircularBuffer *cb, PacketView *p);

#endif /* BUFFER_H */
