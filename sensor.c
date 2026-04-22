#include <stdlib.h>
#include <string.h>

#include "sensor.h"

const char *sensor_name(int id)
{
    switch (id) {
        case SENSOR_TEMP:     return "Temperature";
        case SENSOR_HUMIDITY: return "Humidity";
        case SENSOR_LIGHT:    return "Light";
        default:              return "Unknown";
    }
}

const char *sensor_unit(int id)
{
    switch (id) {
        case SENSOR_TEMP:     return "C";
        case SENSOR_HUMIDITY: return "%";
        case SENSOR_LIGHT:    return "lux";
        default:              return "?";
    }
}

float fake_sensor(int id)
{
    switch (id) {
        case SENSOR_TEMP:     return 10.0f + (float)(rand() % 300) / 10.0f;
        case SENSOR_HUMIDITY: return 20.0f + (float)(rand() % 800) / 10.0f;
        case SENSOR_LIGHT:    return (float)(rand() % 1000);
        default:              return 0.0f;
    }
}

PacketView make_packet(int sensor_id, float value, unsigned short ts)
{
    PacketView pv;
    memset(&pv, 0, sizeof(pv));
    pv.pkt.sensor_id = (unsigned char)sensor_id;
    pv.pkt.timestamp = ts;
    pv.pkt.value     = value;
    return pv;
}
