#ifndef SENSOR_H
#define SENSOR_H

#include "defs.h"

/*
 * SensorPacket — 8 bytes:
 *   sensor_id [1]  flags    [1]
 *   timestamp [2]
 *   value     [4]
 */
typedef struct {
    unsigned char  sensor_id;
    unsigned char  flags;
    unsigned short timestamp;
    float          value;
} SensorPacket;

/*
 * PacketView union: same 8 bytes readable as a structured packet
 * or as a raw byte array — two lenses on one block of memory.
 */
typedef union {
    SensorPacket  pkt;
    unsigned char raw[sizeof(SensorPacket)];
} PacketView;

const char *sensor_name(int id);
const char *sensor_unit(int id);
float       fake_sensor(int id);
PacketView  make_packet(int sensor_id, float value, unsigned short ts);

#endif /* SENSOR_H */
