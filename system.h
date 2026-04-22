#ifndef SYSTEM_H
#define SYSTEM_H

#include "buffer.h"
#include "status.h"

typedef struct {
    CircularBuffer log;
    unsigned char  status_reg;
    unsigned long  tick;
} SystemState;

#endif /* SYSTEM_H */
