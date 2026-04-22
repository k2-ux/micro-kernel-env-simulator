# Micro-Kernel Environmental Simulator

A command-line C program that simulates a small embedded system reading data
from three sensors (Temperature, Humidity, Light), storing readings in a
circular buffer, and controlling actuators through a hardware-style status
register.

The project is structured as a hands-on tour of core C concepts: pointers and
pointer arithmetic, structs and unions, bitwise operations, dynamic memory
allocation, function pointers, and the preprocessor.

## Getting Started

### Cloning the Repository

```bash
git clone https://github.com/k2-ux/micro-kernel-env-simulator.git
cd micro-kernel-env-simulator
```

### Prerequisites

- **Linux / macOS** — any POSIX shell environment works
- `gcc` (GCC 7+ or any C11-compliant compiler such as `clang`)
- `make`

Install on Debian/Ubuntu:

```bash
sudo apt update && sudo apt install -y gcc make
```

Install on macOS (via Homebrew):

```bash
brew install gcc make
```

---

## Building

```bash
make          # production build  →  ./simulator
make debug    # debug build       →  ./simulator_debug  (prints internal trace to stderr)
make clean    # remove binaries and object files
```

Requires `gcc` and `make`. Any C11-compliant compiler works.

---

## Running

```bash
./simulator
```

Type `help` at the prompt for the full command list. A quick tour:

```
sim> read             # sample all three sensors
sim> status           # show the status register and log metadata
sim> dump             # list every entry in the circular log
sim> rawbytes         # show the last entry as raw hex bytes vs. a parsed struct
sim> inject 0 9.0     # inject a synthetic temperature reading of 9.0 C
sim> toggleflag 2     # manually flip the Sensor Error bit
sim> reset            # clear the log and zero the status register
sim> quit
```

---

## Commands

| Command | Description |
|---|---|
| `read` | Sample all three sensors and push readings into the log |
| `inject <id> <val>` | Push a synthetic reading (`0`=Temp `1`=Humidity `2`=Light) |
| `status` | Print the status register (binary + named flags) and log stats |
| `dump` | Print every log entry, oldest first |
| `rawbytes` | Print the last log entry as raw bytes and as a parsed struct |
| `pop` | Remove and display the oldest log entry |
| `setflag <N>` | Set bit N in the status register |
| `clearflag <N>` | Clear bit N in the status register |
| `toggleflag <N>` | XOR-toggle bit N in the status register |
| `reset` | Clear the log and zero the status register |
| `help` | Show the command list |
| `quit` | Shut down and free memory |

---

## File Structure

### `defs.h`
Global constants and the debug macro. Everything that needs to be visible
across multiple modules lives here: `BUFFER_SIZE`, `SENSOR_*` IDs,
`FLAG_*` bitmasks for the status register, and threshold values used by the
processing logic. The `DBG(...)` macro expands to a `fprintf` in debug builds
and to nothing in production — controlled by `-DDEBUG` at compile time.

### `sensor.h` / `sensor.c`
Defines the two core data types:

- `SensorPacket` — an 8-byte struct holding a sensor ID, flags, timestamp,
  and float value.
- `PacketView` — a **union** that overlays the same 8 bytes as either a
  `SensorPacket` or a raw `unsigned char[8]` array. The `rawbytes` command
  uses this to show that both views are the same memory.

Also contains `fake_sensor()` (pseudo-random sensor simulation),
`make_packet()`, and `sensor_name()` / `sensor_unit()` label helpers.

### `buffer.h` / `buffer.c`
A fixed-capacity circular buffer allocated on the heap with `malloc`. The
internal read and write positions are tracked as raw pointers (`head`, `tail`,
`base`) rather than integer indices. Advancing a pointer uses pointer
arithmetic and a bounds check against `base + capacity` to implement the wrap.

Key functions:
- `cb_init` / `cb_free` — heap allocation and deallocation
- `cb_push` — write one entry; overwrites the oldest when full
- `cb_pop` — read and remove the oldest entry
- `cb_next` — advance a pointer one slot with wrap-around

### `status.h` / `status.c`
Thin wrappers around bitwise operations on the single-byte System Status
Register. Keeps the bit manipulation readable and in one place.

- `set_flag` — `|=`
- `clear_flag` — `&= ~mask`
- `test_flag` — `& mask`
- `print_status_reg` — prints the register as hex, binary, and named flags

### `system.h`
Defines `SystemState`, the top-level struct passed to every command handler.
It bundles the `CircularBuffer`, the `status_reg` byte, and the tick counter.
This file has no corresponding `.c` because it contains no logic.

### `commands.h` / `commands.c`
All user-facing logic lives here.

`process_packet()` inspects a fresh sensor reading and updates the status
register — turning the heater on below 15 C, the fan on above 30 C or 70 %
humidity, and night mode below 200 lux.

Each command (`cmd_read`, `cmd_dump`, `cmd_rawbytes`, etc.) is a static
function with the same signature: `void fn(SystemState *, const char *)`.

`COMMAND_TABLE` is an array of `Command` structs, each pairing a name string
with a **function pointer** to its handler. `dispatch()` parses the user's
input, walks the table, and calls the matching pointer — adding a new command
requires no changes to `dispatch` itself.

### `main.c`
Entry point only. Initialises the random seed, calls `cb_init` to allocate the
log buffer on the heap, runs the read-eval loop, then calls `cb_free` before
exiting. No logic that belongs in a module lives here.

---

## Status Register Bit Map

```
Bit 4    Bit 3    Bit 2         Bit 1   Bit 0
Night    Log      Sensor        Fan     Heater
Mode     Full     Error         On      On
```

The register is a single `unsigned char`. All flag manipulation goes through
`set_flag`, `clear_flag`, and `test_flag` in `status.c`.

---

## Key C Concepts and Where to Find Them

| Concept | Location |
|---|---|
| `struct` | `SensorPacket` in `sensor.h` |
| `union` | `PacketView` in `sensor.h`, demonstrated by `cmd_rawbytes` |
| Pointer arithmetic | `cb_next`, `cb_push`, `cb_pop` in `buffer.c`; traversal loop in `cmd_dump` |
| `malloc` / `free` | `cb_init` / `cb_free` in `buffer.c` |
| Bitwise `\|=` `&=` `^=` | `set_flag`, `clear_flag`, `cmd_toggleflag` in `status.c` / `commands.c` |
| Function pointers | `Command.handler` field and `COMMAND_TABLE` in `commands.c` |
| `#define` constants | `defs.h` |
| `#ifdef DEBUG` | `defs.h` (`DBG` macro), `main.c` (build label) |
