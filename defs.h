#ifndef DEFS_H
#define DEFS_H

#define BUFFER_SIZE          16
#define MAX_CMD_LEN          64

#define SENSOR_TEMP           0
#define SENSOR_HUMIDITY       1
#define SENSOR_LIGHT          2

/* System Status Register bit masks */
#define FLAG_HEATER_ON       (1u << 0)
#define FLAG_FAN_ON          (1u << 1)
#define FLAG_SENSOR_ERROR    (1u << 2)
#define FLAG_LOG_FULL        (1u << 3)
#define FLAG_NIGHT_MODE      (1u << 4)

/* Threshold constants */
#define TEMP_HIGH_THRESH     30.0f
#define TEMP_LOW_THRESH      15.0f
#define HUMIDITY_HIGH_THRESH 70.0f
#define LIGHT_LOW_THRESH     200.0f

/* Debug trace: compile with -DDEBUG to activate */
#ifdef DEBUG
#  include <stdio.h>
#  define DBG(fmt, ...) fprintf(stderr, "[DBG] " fmt "\n", ##__VA_ARGS__)
#else
#  define DBG(fmt, ...)
#endif

#endif /* DEFS_H */
