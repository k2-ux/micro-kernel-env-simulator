#ifndef STATUS_H
#define STATUS_H

#include "defs.h"

void set_flag(unsigned char *reg, unsigned char mask);
void clear_flag(unsigned char *reg, unsigned char mask);
int  test_flag(unsigned char reg, unsigned char mask);
void print_status_reg(unsigned char reg);

#endif /* STATUS_H */
