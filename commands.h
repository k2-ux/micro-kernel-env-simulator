#ifndef COMMANDS_H
#define COMMANDS_H

#include "system.h"

typedef struct {
    const char *name;
    void (*handler)(SystemState *, const char *);   /* function pointer */
} Command;

void process_packet(SystemState *sys, const PacketView *pv);

/* Returns 1 to keep running, 0 to quit */
int dispatch(SystemState *sys, const char *input);

#endif /* COMMANDS_H */
