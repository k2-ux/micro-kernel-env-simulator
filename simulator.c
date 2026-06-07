/*
 * Micro-Kernel Environmental Simulator
 *
 * Demonstrates: pointers & arithmetic, struct/union, bitwise ops,
 * dynamic allocation, function pointers, and the preprocessor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * PREPROCESSOR — hardware-like constants and build modes
 * ============================================================ */

#define BUFFER_SIZE          16
#define MAX_CMD_LEN          64

#define SENSOR_TEMP           0
#define SENSOR_HUMIDITY       1
#define SENSOR_LIGHT          2

/* System Status Register bit masks */
#define FLAG_HEATER_ON       (1u << 0)   /* Bit 0 */
#define FLAG_FAN_ON          (1u << 1)   /* Bit 1 */
#define FLAG_SENSOR_ERROR    (1u << 2)   /* Bit 2 */
#define FLAG_LOG_FULL        (1u << 3)   /* Bit 3 */
#define FLAG_NIGHT_MODE      (1u << 4)   /* Bit 4 */

/* Threshold constants */
#define TEMP_HIGH_THRESH     30.0f
#define TEMP_LOW_THRESH      15.0f
#define HUMIDITY_HIGH_THRESH 70.0f
#define LIGHT_LOW_THRESH     200.0f

/* Debug macro: compile with -DDEBUG to activate */
#ifdef DEBUG
#  define DBG(fmt, ...) fprintf(stderr, "[DBG] " fmt "\n", ##__VA_ARGS__)
#else
#  define DBG(fmt, ...)
#endif


/* ============================================================
 * STRUCTURES & UNIONS — sensor packet definition
 * ============================================================ */

/*
 * SensorPacket is exactly 8 bytes:
 *   sensor_id  [1]  + flags     [1]
 *   timestamp  [2]
 *   value      [4]
 */
typedef struct {
    unsigned char  sensor_id;
    unsigned char  flags;
    unsigned short timestamp;
    float          value;
} SensorPacket;

/*
 * PacketView union: lets us inspect the same 8 bytes either as a
 * structured SensorPacket or as a raw byte array — same memory, two lenses.
 */
typedef union {
    SensorPacket pkt;
    unsigned char raw[sizeof(SensorPacket)];
} PacketView;


/* ============================================================
 * CIRCULAR BUFFER — dynamically allocated, pointer-arithmetic traversal
 * ============================================================ */

typedef struct {
    PacketView *data;       /* malloc'd array                    */
    PacketView *base;       /* first element (anchor for arith.) */
    PacketView *head;       /* next write position               */
    PacketView *tail;       /* oldest unread entry               */
    int         count;
    int         capacity;
} CircularBuffer;

/* Allocate the log buffer on the heap */
static int cb_init(CircularBuffer *cb, int capacity)
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

static void cb_free(CircularBuffer *cb)
{
    free(cb->data);
    cb->data = cb->base = cb->head = cb->tail = NULL;
    cb->count = cb->capacity = 0;
}

/* Advance a pointer one slot, wrapping at the end — pure pointer arithmetic */
static PacketView *cb_next(const CircularBuffer *cb, PacketView *p)
{
    PacketView *next = p + 1;                         /* increment by sizeof(PacketView) */
    if (next >= cb->base + cb->capacity)              /* past end? wrap to base          */
        next = cb->base;
    return next;
}

static void cb_push(CircularBuffer *cb, PacketView pv)
{
    *cb->head = pv;
    cb->head  = cb_next(cb, cb->head);

    if (cb->count < cb->capacity) {
        cb->count++;
    } else {
        /* Buffer full: overwrite oldest — advance tail to discard it */
        cb->tail = cb_next(cb, cb->tail);
    }
}

/* Returns 0 on success, -1 when empty */
static int cb_pop(CircularBuffer *cb, PacketView *out)
{
    if (cb->count == 0) return -1;
    *out     = *cb->tail;
    cb->tail = cb_next(cb, cb->tail);
    cb->count--;
    return 0;
}


/* ============================================================
 * SYSTEM STATUS REGISTER — bitwise flag helpers
 * ============================================================ */

static void set_flag(unsigned char *reg, unsigned char mask)   { *reg |=  mask; }
static void clear_flag(unsigned char *reg, unsigned char mask) { *reg &= ~mask; }
static int  test_flag(unsigned char reg, unsigned char mask)   { return (reg & mask) != 0; }

static void print_status_reg(unsigned char reg)
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


/* ============================================================
 * SYSTEM STATE
 * ============================================================ */

typedef struct {
    CircularBuffer log;
    unsigned char  status_reg;
    unsigned long  tick;
} SystemState;


/* ============================================================
 * SENSOR SIMULATION & PROCESSING
 * ============================================================ */

static const char *sensor_name(int id)
{
    switch (id) {
        case SENSOR_TEMP:     return "Temperature";
        case SENSOR_HUMIDITY: return "Humidity";
        case SENSOR_LIGHT:    return "Light";
        default:              return "Unknown";
    }
}

static const char *sensor_unit(int id)
{
    switch (id) {
        case SENSOR_TEMP:     return "C";
        case SENSOR_HUMIDITY: return "%";
        case SENSOR_LIGHT:    return "lux";
        default:              return "?";
    }
}

static float fake_sensor(int id)
{
    switch (id) {
        case SENSOR_TEMP:     return 10.0f + (float)(rand() % 300) / 10.0f; /* 10–40 C   */
        case SENSOR_HUMIDITY: return 20.0f + (float)(rand() % 800) / 10.0f; /* 20–100 %  */
        case SENSOR_LIGHT:    return (float)(rand() % 1000);                 /* 0–999 lux */
        default:              return 0.0f;
    }
}

static PacketView make_packet(int sensor_id, float value, unsigned short ts)
{
    PacketView pv;
    memset(&pv, 0, sizeof(pv));
    pv.pkt.sensor_id = (unsigned char)sensor_id;
    pv.pkt.timestamp = ts;
    pv.pkt.value     = value;
    return pv;
}

/* Update the status register according to sensor reading */
static void process_packet(SystemState *sys, const PacketView *pv)
{
    float val = pv->pkt.value;

    switch (pv->pkt.sensor_id) {
        case SENSOR_TEMP:
            if (val > TEMP_HIGH_THRESH)
                set_flag(&sys->status_reg, FLAG_FAN_ON);
            else
                clear_flag(&sys->status_reg, FLAG_FAN_ON);

            if (val < TEMP_LOW_THRESH)
                set_flag(&sys->status_reg, FLAG_HEATER_ON);
            else
                clear_flag(&sys->status_reg, FLAG_HEATER_ON);
            break;

        case SENSOR_HUMIDITY:
            if (val > HUMIDITY_HIGH_THRESH)
                set_flag(&sys->status_reg, FLAG_FAN_ON);
            break;

        case SENSOR_LIGHT:
            if (val < LIGHT_LOW_THRESH)
                set_flag(&sys->status_reg, FLAG_NIGHT_MODE);
            else
                clear_flag(&sys->status_reg, FLAG_NIGHT_MODE);
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

/* Forward declaration — the dispatcher type is used in the table */
typedef struct SystemState SystemState_t;

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
     * Traverse the live entries using pointer arithmetic:
     * start at tail, step forward modulo capacity until all entries are visited.
     */
    PacketView *ptr = sys->log.tail;
    for (int i = 0; i < sys->log.count; i++) {
        printf("  [%2d] ts=%-5u %-12s %7.2f %s\n",
               i,
               ptr->pkt.timestamp,
               sensor_name(ptr->pkt.sensor_id),
               ptr->pkt.value,
               sensor_unit(ptr->pkt.sensor_id));

        /* Manual pointer-arithmetic step with wrap */
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

    /* Locate the most recently written slot: one step behind head */
    PacketView *last = sys->log.head - 1;
    if (last < sys->log.base)
        last = sys->log.base + sys->log.capacity - 1;

    printf("  SensorPacket occupies %zu bytes:\n", sizeof(SensorPacket));

    /* Walk raw[] via a byte pointer — classic pointer arithmetic over a union */
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
    if (bit < 0 || bit > 7) {
        printf("Usage: setflag <bit 0-7>\n");
        return;
    }
    set_flag(&sys->status_reg, (unsigned char)(1u << bit));
    printf("  Bit %d set.  Status: 0x%02X\n", bit, sys->status_reg);
}

static void cmd_clearflag(SystemState *sys, const char *args)
{
    int bit = args ? atoi(args) : -1;
    if (bit < 0 || bit > 7) {
        printf("Usage: clearflag <bit 0-7>\n");
        return;
    }
    clear_flag(&sys->status_reg, (unsigned char)(1u << bit));
    printf("  Bit %d cleared.  Status: 0x%02X\n", bit, sys->status_reg);
}

static void cmd_toggleflag(SystemState *sys, const char *args)
{
    int bit = args ? atoi(args) : -1;
    if (bit < 0 || bit > 7) {
        printf("Usage: toggleflag <bit 0-7>\n");
        return;
    }
    sys->status_reg ^= (unsigned char)(1u << bit);  /* XOR toggle */
    printf("  Bit %d toggled.  Status: 0x%02X\n", bit, sys->status_reg);
}

static void cmd_reset(SystemState *sys, const char *args)
{
    (void)args;
    sys->log.head  = sys->log.base;
    sys->log.tail  = sys->log.base;
    sys->log.count = 0;
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
 * COMMAND DISPATCHER — function pointer table
 * ============================================================ */

typedef struct {
    const char *name;
    void (*handler)(SystemState *, const char *);  /* function pointer */
} Command;

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

/*
 * Parse the input line, look up the command by name, and call its
 * function pointer.  Returns 1 to keep running, 0 to quit.
 */
static int dispatch(SystemState *sys, const char *input)
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


/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    srand((unsigned int)time(NULL));

    SystemState sys;
    sys.status_reg = 0x00;
    sys.tick       = 0;

    /* Dynamic allocation: the circular buffer lives on the heap */
    if (cb_init(&sys.log, BUFFER_SIZE) != 0) {
        fprintf(stderr, "FATAL: could not allocate log buffer\n");
        return 1;
    }

    printf("==============================================\n");
    printf("   Micro-Kernel Environmental Simulator\n");
#ifdef DEBUG
    printf("   Build : DEBUG\n");
#else
    printf("   Build : PRODUCTION\n");
#endif
    printf("   Log   : %d slots  (%zu bytes on heap)\n",
           BUFFER_SIZE, (size_t)BUFFER_SIZE * sizeof(PacketView));
    printf("==============================================\n");
    printf("Type 'help' for a list of commands.\n\n");

    char line[MAX_CMD_LEN];
    while (1) {
        printf("sim> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        line[strcspn(line, "\n")] = '\0';   /* strip newline */

        if (!dispatch(&sys, line))
            break;
    }

    /* Release the heap-allocated log buffer */
    cb_free(&sys.log);
    printf("Simulator shutdown. Memory freed.\n");
    return 0;
}
