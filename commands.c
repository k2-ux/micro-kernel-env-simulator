#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "sensor.h"
#include "buffer.h"
#include "status.h"
#include "defs.h"

/* ============================================================
 * PROCESSING — update the status register from a sensor reading
 * ============================================================ */

void process_packet(SystemState *sys, const PacketView *pv)
{
    float val = pv->pkt.value;

    switch (pv->pkt.sensor_id) {
        case SENSOR_TEMP:
            if (val > TEMP_HIGH_THRESH)       set_flag(&sys->status_reg, FLAG_FAN_ON);
            else                              clear_flag(&sys->status_reg, FLAG_FAN_ON);
            if (val < TEMP_LOW_THRESH)        set_flag(&sys->status_reg, FLAG_HEATER_ON);
            else                              clear_flag(&sys->status_reg, FLAG_HEATER_ON);
            break;

        case SENSOR_HUMIDITY:
            if (val > HUMIDITY_HIGH_THRESH)   set_flag(&sys->status_reg, FLAG_FAN_ON);
            break;

        case SENSOR_LIGHT:
            if (val < LIGHT_LOW_THRESH)       set_flag(&sys->status_reg, FLAG_NIGHT_MODE);
            else                              clear_flag(&sys->status_reg, FLAG_NIGHT_MODE);
            break;
    }

    if (sys->log.count >= sys->log.capacity)
        set_flag(&sys->status_reg, FLAG_LOG_FULL);
    else
        clear_flag(&sys->status_reg, FLAG_LOG_FULL);

    DBG("processed sensor=%d val=%.2f status=0x%02X",
        pv->pkt.sensor_id, val, sys->status_reg);
}

/* ============================================================
 * COMMAND HANDLERS
 * ============================================================ */

static void cmd_read(SystemState *sys, const char *args)
{
    (void)args;
    printf("=== Sensor Read (tick=%lu) ===\n", sys->tick);

    for (int i = 0; i < 3; i++) {
        float val = fake_sensor(i);
        PacketView pv = make_packet(i, val, (unsigned short)(sys->tick & 0xFFFFu));
        cb_push(&sys->log, pv);
        process_packet(sys, &pv);
        printf("  %-12s : %7.2f %s\n", sensor_name(i), val, sensor_unit(i));
    }
    sys->tick++;
}

static void cmd_inject(SystemState *sys, const char *args)
{
    int   sensor_id = 0;
    float value     = 0.0f;

    if (!args || sscanf(args, "%d %f", &sensor_id, &value) != 2
             || sensor_id < 0 || sensor_id > 2) {
        printf("Usage: inject <sensor 0-2> <value>\n");
        printf("  0=Temperature  1=Humidity  2=Light\n");
        return;
    }

    PacketView pv = make_packet(sensor_id, value, (unsigned short)(sys->tick & 0xFFFFu));
    cb_push(&sys->log, pv);
    process_packet(sys, &pv);
    printf("  Injected: %s = %.2f %s\n",
           sensor_name(sensor_id), value, sensor_unit(sensor_id));
}

static void cmd_status(SystemState *sys, const char *args)
{
    (void)args;
    printf("=== System Status ===\n");
    print_status_reg(sys->status_reg);
    printf("  Log entries     : %d / %d\n", sys->log.count, sys->log.capacity);
    printf("  System tick     : %lu\n", sys->tick);
}

static void cmd_dump(SystemState *sys, const char *args)
{
    (void)args;
    printf("=== Log Dump (%d / %d entries) ===\n", sys->log.count, sys->log.capacity);

    if (sys->log.count == 0) {
        printf("  (empty)\n");
        return;
    }

    /*
     * Traverse live entries via pointer arithmetic:
     * start at tail, step forward wrapping at capacity.
     */
    PacketView *ptr = sys->log.tail;
    for (int i = 0; i < sys->log.count; i++) {
        printf("  [%2d] ts=%-5u %-12s %7.2f %s\n",
               i,
               ptr->pkt.timestamp,
               sensor_name(ptr->pkt.sensor_id),
               ptr->pkt.value,
               sensor_unit(ptr->pkt.sensor_id));

        ptr = sys->log.base + ((ptr - sys->log.base + 1) % sys->log.capacity);
    }
}

static void cmd_rawbytes(SystemState *sys, const char *args)
{
    (void)args;
    printf("=== Raw Byte View of Last Log Entry ===\n");

    if (sys->log.count == 0) {
        printf("  (log is empty)\n");
        return;
    }

    /* Most recently written slot is one step behind head */
    PacketView *last = sys->log.head - 1;
    if (last < sys->log.base)
        last = sys->log.base + sys->log.capacity - 1;

    printf("  SensorPacket occupies %zu bytes:\n", sizeof(SensorPacket));

    /* Walk raw[] via a byte pointer — pointer arithmetic over the union */
    unsigned char *byte_ptr = last->raw;
    for (size_t i = 0; i < sizeof(SensorPacket); i++, byte_ptr++) {
        printf("    raw[%zu] = 0x%02X  (dec %3d)\n", i, *byte_ptr, *byte_ptr);
    }

    printf("  Interpreted as SensorPacket:\n");
    printf("    .sensor_id = %u  (%s)\n", last->pkt.sensor_id, sensor_name(last->pkt.sensor_id));
    printf("    .flags     = 0x%02X\n",    last->pkt.flags);
    printf("    .timestamp = %u\n",        last->pkt.timestamp);
    printf("    .value     = %.4f\n",      last->pkt.value);
}

static void cmd_pop(SystemState *sys, const char *args)
{
    (void)args;
    PacketView pv;
    if (cb_pop(&sys->log, &pv) != 0) {
        printf("  Log is empty.\n");
        return;
    }
    printf("  Popped: ts=%-5u %-12s %.2f %s\n",
           pv.pkt.timestamp,
           sensor_name(pv.pkt.sensor_id),
           pv.pkt.value,
           sensor_unit(pv.pkt.sensor_id));
}

static void cmd_setflag(SystemState *sys, const char *args)
{
    int bit = args ? atoi(args) : -1;
    if (bit < 0 || bit > 7) { printf("Usage: setflag <bit 0-7>\n"); return; }
    set_flag(&sys->status_reg, (unsigned char)(1u << bit));
    printf("  Bit %d set.  Status: 0x%02X\n", bit, sys->status_reg);
}

static void cmd_clearflag(SystemState *sys, const char *args)
{
    int bit = args ? atoi(args) : -1;
    if (bit < 0 || bit > 7) { printf("Usage: clearflag <bit 0-7>\n"); return; }
    clear_flag(&sys->status_reg, (unsigned char)(1u << bit));
    printf("  Bit %d cleared.  Status: 0x%02X\n", bit, sys->status_reg);
}

static void cmd_toggleflag(SystemState *sys, const char *args)
{
    int bit = args ? atoi(args) : -1;
    if (bit < 0 || bit > 7) { printf("Usage: toggleflag <bit 0-7>\n"); return; }
    sys->status_reg ^= (unsigned char)(1u << bit);   /* XOR toggle */
    printf("  Bit %d toggled.  Status: 0x%02X\n", bit, sys->status_reg);
}

static void cmd_reset(SystemState *sys, const char *args)
{
    (void)args;
    sys->log.head   = sys->log.base;
    sys->log.tail   = sys->log.base;
    sys->log.count  = 0;
    sys->status_reg = 0x00;
    sys->tick       = 0;
    printf("  System reset. Log cleared, status register zeroed.\n");
}

static void cmd_help(SystemState *sys, const char *args)
{
    (void)sys; (void)args;
    printf("Commands:\n");
    printf("  read              Sample all three sensors and log readings\n");
    printf("  inject <id> <val> Inject a synthetic reading (id: 0=Temp 1=Hum 2=Light)\n");
    printf("  status            Show status register and log metadata\n");
    printf("  dump              Dump all log entries (oldest first)\n");
    printf("  rawbytes          Show union raw-byte view of the last log entry\n");
    printf("  pop               Remove and display the oldest log entry\n");
    printf("  setflag   <N>     Set bit N in the status register\n");
    printf("  clearflag <N>     Clear bit N in the status register\n");
    printf("  toggleflag <N>    XOR-toggle bit N in the status register\n");
    printf("  reset             Clear log and zero status register\n");
    printf("  help              Show this help\n");
    printf("  quit              Shut down the simulator\n");
}

/* ============================================================
 * COMMAND TABLE — array of function pointers
 * ============================================================ */

static const Command COMMAND_TABLE[] = {
    { "read",       cmd_read       },
    { "inject",     cmd_inject     },
    { "status",     cmd_status     },
    { "dump",       cmd_dump       },
    { "rawbytes",   cmd_rawbytes   },
    { "pop",        cmd_pop        },
    { "setflag",    cmd_setflag    },
    { "clearflag",  cmd_clearflag  },
    { "toggleflag", cmd_toggleflag },
    { "reset",      cmd_reset      },
    { "help",       cmd_help       },
};

#define NUM_COMMANDS (sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]))

int dispatch(SystemState *sys, const char *input)
{
    char cmd[MAX_CMD_LEN] = {0};
    char arg[MAX_CMD_LEN] = {0};

    sscanf(input, "%63s %63[^\n]", cmd, arg);

    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
        return 0;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(cmd, COMMAND_TABLE[i].name) == 0) {
            DBG("dispatch -> %s (arg='%s')", cmd, arg);
            COMMAND_TABLE[i].handler(sys, arg[0] ? arg : NULL);
            return 1;
        }
    }

    if (cmd[0] != '\0')
        printf("  Unknown command '%s'. Type 'help'.\n", cmd);
    return 1;
}
