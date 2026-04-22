#include <stdio.h>

#include "status.h"

void set_flag(unsigned char *reg, unsigned char mask)  { *reg |=  mask; }
void clear_flag(unsigned char *reg, unsigned char mask) { *reg &= ~mask; }
int  test_flag(unsigned char reg, unsigned char mask)  { return (reg & mask) != 0; }

void print_status_reg(unsigned char reg)
{
    printf("  Status Register : 0x%02X  (binary: ", reg);
    for (int b = 7; b >= 0; b--)
        putchar((reg >> b) & 1 ? '1' : '0');
    printf(")\n");
    printf("    [%c] Bit 0 - Heater On\n",    test_flag(reg, FLAG_HEATER_ON)    ? 'X' : ' ');
    printf("    [%c] Bit 1 - Fan On\n",        test_flag(reg, FLAG_FAN_ON)       ? 'X' : ' ');
    printf("    [%c] Bit 2 - Sensor Error\n",  test_flag(reg, FLAG_SENSOR_ERROR) ? 'X' : ' ');
    printf("    [%c] Bit 3 - Log Full\n",      test_flag(reg, FLAG_LOG_FULL)     ? 'X' : ' ');
    printf("    [%c] Bit 4 - Night Mode\n",    test_flag(reg, FLAG_NIGHT_MODE)   ? 'X' : ' ');
}
