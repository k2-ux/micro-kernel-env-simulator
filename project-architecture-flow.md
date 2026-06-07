# Micro-Kernel Environmental Simulator — Architecture & Flow Document

## 1. Executive Summary

**What this app does:**
The Micro-Kernel Environmental Simulator is a single-process, interactive command-line application written in C11. It emulates the runtime behavior of a bare-metal embedded firmware system: three virtual sensors (temperature, humidity, light) generate readings, each reading is packed into a binary sensor packet and written into a fixed-capacity circular log buffer on the heap, and a one-byte status register tracks system state via hardware-style bitfield flags. The user drives the simulation through a REPL (Read-Eval-Print Loop) that accepts structured commands and produces formatted output — mirroring how a firmware engineer would interact with a microcontroller via a serial debug console.

The project's primary educational purpose is to demonstrate, in a single coherent program, the five foundational C systems-programming concepts: pointer arithmetic, struct/union memory layout, bitwise operations on a hardware register, dynamic heap allocation with manual lifecycle management, and function-pointer dispatch tables. It exists in two forms: a modular multi-file build (`main.c` + four modules) and a single-file self-contained reference (`simulator.c`).

**Tech Stack:**

| Layer | Technology |
|---|---|
| Language | C (ISO C11) |
| Build System | GNU Make |
| Compiler | GCC (`-Wall -Wextra -Wpedantic -std=c11`) |
| Standard Libraries | `stdio.h`, `stdlib.h`, `string.h`, `time.h` |
| Memory Model | Manual heap allocation (`malloc` / `free`) |
| Randomness | `rand()` seeded with `time(NULL)` |
| Debug Instrumentation | Conditional `DBG()` macro (enabled with `-DDEBUG` at compile time) |
| External Dependencies | None |

---

## 2. High-Level Data Flow

The system operates as a tight request/response loop. Every user interaction follows the same pipeline:

```
User keyboard input
       │
       ▼
┌─────────────────────────────────────────┐
│  REPL (main.c)                          │
│  fgets() → strip newline → dispatch()   │
└──────────────────┬──────────────────────┘
                   │  raw input string
                   ▼
┌─────────────────────────────────────────┐
│  Dispatcher (commands.c: dispatch())    │
│  sscanf → cmd name + arg string         │
│  Linear scan of COMMAND_TABLE[]         │
│  → calls handler via function pointer   │
└──────────────────┬──────────────────────┘
                   │  SystemState *sys + arg string
                   ▼
┌─────────────────────────────────────────┐
│  Command Handler (commands.c)           │
│  e.g. cmd_read, cmd_inject, cmd_status  │
└────┬─────────────┬────────────┬─────────┘
     │             │            │
     ▼             ▼            ▼
┌─────────┐  ┌──────────┐  ┌──────────────┐
│ Sensor  │  │ Circular │  │ Status       │
│ Module  │  │ Buffer   │  │ Register     │
│(sensor.c│  │(buffer.c)│  │(status.c)    │
│fake_    │  │cb_push() │  │set_flag()    │
│sensor() │  │cb_pop()  │  │clear_flag()  │
│make_    │  │cb_next() │  │test_flag()   │
│packet() │  └──────────┘  └──────────────┘
└─────────┘
     │             │            │
     └─────────────┴────────────┘
                   │  formatted output
                   ▼
            stdout (terminal)
```

**Step-by-step narrative for a `read` command:**

1. The user types `read` and presses Enter at the `sim>` prompt.
2. `main.c:fgets()` reads the line into a fixed `char line[64]` stack buffer. `strcspn` strips the trailing newline in-place.
3. `dispatch()` in `commands.c` calls `sscanf` to split the string into a command token (`"read"`) and an argument string (empty). It performs a linear scan of the `COMMAND_TABLE[]` array comparing each `.name` field using `strcmp`.
4. A match is found at index 0. The `.handler` function pointer is dereferenced and `cmd_read(sys, NULL)` is called.
5. `cmd_read` loops over sensor IDs 0, 1, 2. For each, it calls `fake_sensor(i)` in `sensor.c`, which uses `rand()` to generate a float in a realistic range.
6. `make_packet(i, val, tick)` constructs a `PacketView` union: it `memset`s the 8 bytes to zero, then writes `sensor_id`, `timestamp`, and `value` into the `SensorPacket` struct member. The `raw[8]` byte-array member of the union now reflects the same bytes.
7. `cb_push(&sys->log, pv)` in `buffer.c` writes the `PacketView` to the current `head` slot of the heap-allocated array, advances `head` using `cb_next()` (pointer arithmetic with wraparound), and increments `count` — or advances `tail` if the buffer was already full.
8. `process_packet(sys, &pv)` in `commands.c` reads `pv->pkt.value` and `pv->pkt.sensor_id`, then calls `set_flag()` or `clear_flag()` in `status.c` to update the single-byte `sys->status_reg` using bitwise OR / AND-NOT operations.
9. The sensor name, value, and unit are printed to stdout.
10. Control returns to `main.c`, `sys->tick` is incremented, and the REPL prints `sim>` again.

---

## 3. Core Modules Breakdown

---

### Module: Constants & Build Configuration

**Responsibility:** Centralises every magic number, flag mask, threshold value, and build-mode switch. All other modules include this header. Changing a value here affects the whole system — no scattered literals.

**Key Files:** `defs.h`

**How it works:**
- `BUFFER_SIZE 16` and `MAX_CMD_LEN 64` are preprocessor constants controlling heap allocation and stack buffer sizes respectively.
- `SENSOR_TEMP 0`, `SENSOR_HUMIDITY 1`, `SENSOR_LIGHT 2` are sensor ID constants used as `switch` discriminants throughout the codebase.
- The five `FLAG_*` constants are bitmasks generated via `1u << N` (bits 0–4 of the status register). Using unsigned integer shift avoids undefined behavior on signed types.
- Four threshold constants (`TEMP_HIGH_THRESH`, `TEMP_LOW_THRESH`, `HUMIDITY_HIGH_THRESH`, `LIGHT_LOW_THRESH`) are `float` literals that drive the actuator logic in `process_packet()`.
- The `DBG(fmt, ...)` macro expands to a `fprintf(stderr, ...)` call only when the translation unit is compiled with `-DDEBUG`. In production builds, the macro expands to nothing — zero runtime cost, zero binary footprint.

---

### Module: Sensor Simulation

**Responsibility:** Owns all knowledge about what sensors exist, what they are named, what units they produce, and how to generate fake readings from them. Also constructs binary `PacketView` objects that travel into the circular buffer.

**Key Files:** `sensor.h`, `sensor.c`

**How it works:**
- `sensor_name(int id)` and `sensor_unit(int id)` are simple `switch` lookup functions returning string literals. They both have a `default` case returning `"Unknown"` / `"?"` as a safety net for out-of-range IDs.
- `fake_sensor(int id)` generates pseudo-random floats in physically plausible ranges using `rand() % N` scaled to float. Temperature: 10–40 °C. Humidity: 20–100 %. Light: 0–999 lux. The `default` case returns `0.0f`.
- `make_packet(int sensor_id, float value, unsigned short ts)` constructs a `PacketView`. It zeroes the full 8-byte union with `memset` first (preventing garbage in the unused `flags` field), then sets the three populated fields. The union is returned by value — a full 8-byte copy on the stack.
- The `SensorPacket` struct is carefully laid out to be exactly 8 bytes: 1 (`sensor_id`) + 1 (`flags`) + 2 (`timestamp`) + 4 (`value` as IEEE 754 float) = 8. The `PacketView` union overlays `SensorPacket` with `unsigned char raw[8]`, providing a raw-byte lens on the same memory — used by the `rawbytes` command to inspect packet memory at the byte level.

---

### Module: Circular Buffer (Ring Log)

**Responsibility:** Provides a fixed-capacity, heap-allocated FIFO queue of `PacketView` entries. Manages all pointer arithmetic for wraparound traversal. Older entries are silently overwritten when the buffer is full — a deliberate embedded-systems design choice (no blocking, no dynamic growth).

**Key Files:** `buffer.h`, `buffer.c`

**How it works:**
- `cb_init(CircularBuffer *cb, int capacity)` calls `malloc(capacity * sizeof(PacketView))` and initialises `base`, `head`, and `tail` to all point at the start of the allocated block. `count = 0`. Returns `-1` on allocation failure.
- The struct stores both `data` (the raw malloc pointer, kept for `free()`) and `base` (the arithmetic anchor). In this implementation they are always equal, but separating them makes the intent explicit.
- `cb_next(cb, p)` is the single wraparound primitive: `p + 1` in pointer arithmetic advances by `sizeof(PacketView)` bytes. If the result equals or exceeds `base + capacity`, it resets to `base`. Every write and traversal delegates to this function — the wrap logic exists in exactly one place.
- `cb_push(cb, pv)` writes to `*head` and advances `head`. If `count < capacity`, it increments `count`. If full, it instead advances `tail`, discarding the oldest entry with no warning.
- `cb_pop(cb, out)` reads from `*tail`, advances `tail`, decrements `count`. Returns `-1` on empty.
- `cb_free(cb)` calls `free(cb->data)` and nulls all four pointers plus both integers — a defensive practice preventing use-after-free.

---

### Module: Status Register

**Responsibility:** Encapsulates all bitwise read/write operations on the single-byte hardware-style status register. Provides a human-readable rendering of its current state.

**Key Files:** `status.h`, `status.c`

**How it works:**
- `set_flag(reg, mask)`: `*reg |= mask` — sets specific bits using bitwise OR.
- `clear_flag(reg, mask)`: `*reg &= ~mask` — clears specific bits using AND with the bitwise complement.
- `test_flag(reg, mask)`: `(reg & mask) != 0` — returns 1 if any masked bit is set, 0 otherwise.
- `print_status_reg(reg)` renders the register in three forms: hex (`0x%02X`), 8-bit binary (a loop shifting from bit 7 down to bit 0), and named flag lines for bits 0–4. Bits 5–7 have no named label in the current implementation — they render silently.
- All three mutation functions take a pointer to the register (`unsigned char *reg`) so they modify the caller's copy in place. `test_flag` takes by value since it only reads.

---

### Module: Command Dispatch & Business Logic

**Responsibility:** The central orchestration layer. Owns the REPL's command-parsing logic, all individual command handler functions, and the function-pointer dispatch table that connects command names to handlers. Also owns `process_packet()` — the rule engine that translates sensor readings into status register changes.

**Key Files:** `commands.h`, `commands.c`

**How it works:**

**Dispatch mechanism:** `dispatch(sys, input)` uses `sscanf(input, "%63s %63[^\n]", cmd, arg)` to split input into a verb and an optional remainder. It first checks for `"quit"` / `"exit"` (returning 0 to signal shutdown) before entering a `for` loop over `COMMAND_TABLE[]`. Each element is a `Command` struct — a name string and a `void (*handler)(SystemState *, const char *)` function pointer. On a match, the handler is called via the pointer with `arg[0] ? arg : NULL` (passing `NULL` if the argument string is empty). If no match is found and the input is non-empty, an "Unknown command" message is printed.

**`COMMAND_TABLE[]`** is a `static const` array of 11 entries. Its size is computed at compile time with the `sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0])` idiom, stored in `NUM_COMMANDS`. Adding a new command requires only: a handler function, one new table row, and one help string.

**`process_packet()`** is the rule engine: it reads `sensor_id` and `value` from a packet and updates flags accordingly. Temperature controls both `FLAG_FAN_ON` (above 30°C) and `FLAG_HEATER_ON` (below 15°C). Humidity can set `FLAG_FAN_ON` (above 70%) but never clears it (by design — the fan could be on for temperature reasons). Light controls `FLAG_NIGHT_MODE`. After every sensor evaluation, `FLAG_LOG_FULL` is set or cleared by comparing `log.count` to `log.capacity`.

---

### Module: System State & Type Definitions

**Responsibility:** Defines the top-level composite type that is threaded through every command handler, and aggregates all module headers into a single include point.

**Key Files:** `system.h`

**How it works:**
- `SystemState` contains three fields: `CircularBuffer log` (the log, embedded by value — the buffer's *heap pointer* lives inside this struct, but the struct itself is on the stack in `main`), `unsigned char status_reg` (the 8-bit hardware-style register), and `unsigned long tick` (a monotonically incrementing counter used as a timestamp source).
- `system.h` includes both `buffer.h` and `status.h`, giving every file that includes `commands.h` → `system.h` access to the full type graph without redundant includes.

---

### Module: Entry Point & REPL

**Responsibility:** Program lifecycle: initialise the system, run the interactive loop, and tear down on exit.

**Key Files:** `main.c`

**How it works:**
- Seeds `rand()` with `time(NULL)` for non-deterministic sensor values across runs.
- Declares `SystemState sys` on the stack, zeroes `status_reg` and `tick` explicitly (struct members are not zero-initialised by default in C).
- Calls `cb_init(&sys.log, BUFFER_SIZE)` — the single heap allocation in the program. Checks the return value; on failure, prints to `stderr` and returns exit code 1.
- Prints a startup banner that includes the build mode (`DEBUG` vs `PRODUCTION`) and the log's heap footprint (`BUFFER_SIZE * sizeof(PacketView)` bytes).
- Runs the REPL: `fgets` into a `char line[MAX_CMD_LEN]` stack buffer, `strcspn` strip of the newline, `dispatch()` call. Loops until `dispatch()` returns 0 (quit/exit) or `fgets` returns NULL (EOF / stdin closed).
- Calls `cb_free(&sys.log)` unconditionally on exit, then prints a shutdown message.

---

### Module: Monolithic Reference Build

**Responsibility:** A self-contained, single-file version of the entire system. Serves as a portable reference and a teaching artifact showing how the multi-file architecture collapses into one translation unit.

**Key Files:** `simulator.c`

**How it works:** Contains all struct definitions, all utility functions (marked `static`), all command handlers, the dispatch table, and `main()` in one file. Functionally identical to the multi-file build. The Makefile does not include it in the default `all` target — it must be compiled directly (`gcc -std=c11 simulator.c -o simulator_mono`).

---

## 4. User Flows & Use Cases

---

### Flow: System Startup

**Happy Path:**
1. User runs `./simulator` (or `./simulator_debug` for the debug build).
2. `srand` seeds the PRNG with the current epoch second.
3. `cb_init` allocates `16 * sizeof(PacketView)` = 128 bytes on the heap.
4. Banner prints: build mode, log slot count, heap byte count.
5. `sim>` prompt appears. System is ready with `status_reg = 0x00`, `tick = 0`, empty log.

**Edge Cases & Error Handling:**
- If `malloc` fails inside `cb_init` (returns NULL), `cb_init` returns `-1`. `main.c` catches this, prints `FATAL: could not allocate log buffer` to `stderr`, and exits with code 1. No further initialisation occurs, no partial state is left behind.
- If the binary is run in a constrained environment where `time(NULL)` returns the same value every second (e.g., a scripted test harness), all runs in that second will produce identical `rand()` sequences. This is a known property of the seeding strategy, not a bug.

---

### Flow: Sampling All Sensors (`read`)

**Happy Path:**
1. User types `read`.
2. `cmd_read` loops i = 0, 1, 2.
3. For each i: `fake_sensor(i)` generates a float, `make_packet` packs it into a `PacketView`, `cb_push` writes it to the log, `process_packet` updates the status register, and the reading is printed: `Temperature :   27.43 C`.
4. `sys->tick` is incremented.

**Edge Cases & Error Handling:**
- **Buffer full (after 16 `read` cycles, 48 entries attempted):** `cb_push` silently overwrites the oldest entry by advancing `tail`. `FLAG_LOG_FULL` (bit 3) is set by `process_packet` on every subsequent push. The log never grows beyond `BUFFER_SIZE` — no realloc, no crash.
- **Fan flag conflict between temperature and humidity:** If temperature is below the high threshold but humidity is above 70%, `process_packet` for the humidity packet sets `FLAG_FAN_ON`. If the next temperature reading is then below the high threshold, `process_packet` for temperature calls `clear_flag(&sys->status_reg, FLAG_FAN_ON)` — overriding the humidity-driven flag. This is the only state-clobbering edge case in the rule engine; it is a design characteristic, not a bug.
- **Boundary values:** All threshold comparisons are strict (`>` or `<`), not `>=` or `<=`. A temperature reading of exactly `30.0f` does **not** set `FLAG_FAN_ON`. A reading of exactly `15.0f` does **not** set `FLAG_HEATER_ON`.

---

### Flow: Injecting a Synthetic Reading (`inject`)

**Happy Path:**
1. User types `inject 0 35.5`.
2. `cmd_inject` calls `sscanf(args, "%d %f", &sensor_id, &value)` — parses `sensor_id = 0`, `value = 35.5`.
3. A packet is built with the current `sys->tick` as timestamp.
4. Packet is pushed to the log and processed. Since 35.5 > 30.0 (TEMP_HIGH_THRESH), `FLAG_FAN_ON` is set.
5. Output: `Injected: Temperature = 35.50 C`.

**Edge Cases & Error Handling:**
- **Missing arguments (`inject` with no args):** `args` is `NULL` (dispatcher passes NULL when arg string is empty). The `!args` check triggers immediately, printing the usage message. `sscanf` is never called.
- **Too few arguments (`inject 0`):** `sscanf` returns 1 (parsed only one value). The `!= 2` check triggers the usage message. No packet is created.
- **Invalid sensor ID (`inject 5 50.0`):** `sscanf` returns 2 successfully, but `sensor_id > 2` is true. The usage message is printed. No packet is created or logged.
- **Non-numeric input (`inject abc xyz`):** `sscanf` returns 0 or an unexpected parse, triggering the `!= 2` check and the usage message.
- **Negative sensor ID (`inject -1 20.0`):** `sensor_id < 0` check triggers the usage message.

---

### Flow: Inspecting System State (`status`)

**Happy Path:**
1. User types `status`.
2. `cmd_status` calls `print_status_reg(sys->status_reg)` which renders the byte as hex, then binary, then five named flag lines with `[X]` or `[ ]` markers.
3. Prints log occupancy (`count / capacity`) and current `tick`.

**Edge Cases & Error Handling:**
- No error paths. `status` has no arguments and reads only from `SystemState` — it cannot fail. Bits 5, 6, 7 of the status register have no named label in `print_status_reg`; they can be set via `setflag` but will not appear in the named flag list. Their state is still visible in the hex and binary output lines.

---

### Flow: Dumping the Log (`dump`)

**Happy Path:**
1. User types `dump`.
2. `cmd_dump` starts a pointer `ptr` at `sys->log.tail` (the oldest entry).
3. Loops `sys->log.count` times, printing timestamp, sensor name, value, and unit for each `PacketView`.
4. Pointer is advanced each iteration using modulo arithmetic: `sys->log.base + ((ptr - sys->log.base + 1) % sys->log.capacity)`.

**Edge Cases & Error Handling:**
- **Empty log:** `sys->log.count == 0` is checked at the top. Prints `(empty)` and returns early. The traversal loop is never entered, preventing a traversal starting from an uninitialised `tail` pointing at `base` with garbage data.
- **Partially-filled log:** The loop runs exactly `count` times regardless of `capacity`, so it only visits live entries — never unwritten slots.
- **Full log with wraparound:** If 20 readings have been pushed into a 16-slot buffer, `tail` will have advanced 4 positions past `base`. The dump still prints exactly 16 entries in correct oldest-first order because it starts at `tail` and uses the modulo step.

---

### Flow: Viewing Raw Packet Bytes (`rawbytes`)

**Happy Path:**
1. User types `rawbytes`.
2. `cmd_rawbytes` computes a pointer to the most recently written slot: `sys->log.head - 1` with a wrap-around check if `head` is at `base`.
3. Prints `sizeof(SensorPacket)` (8) bytes from `last->raw[]` as `raw[i] = 0xXX (dec NNN)`.
4. Then re-interprets the same memory as a `SensorPacket` and prints each field by name.

**Edge Cases & Error Handling:**
- **Empty log:** `sys->log.count == 0` guard prints `(log is empty)` and returns. This prevents dereferencing a stale pointer before any data has been written.
- **Head at base (wraparound):** `sys->log.head - 1` would point before the allocated block. The explicit check `if (last < sys->log.base)` corrects this to `sys->log.base + capacity - 1`, pointing to the last slot in the array.

---

### Flow: Removing the Oldest Entry (`pop`)

**Happy Path:**
1. User types `pop`.
2. `cb_pop` copies `*tail` into a local `PacketView`, advances `tail`, decrements `count`, returns 0.
3. `cmd_pop` prints the popped entry's fields.

**Edge Cases & Error Handling:**
- **Empty log:** `cb_pop` returns `-1`. `cmd_pop` checks the return value and prints `Log is empty.` No memory is read or modified.

---

### Flow: Manipulating the Status Register (`setflag`, `clearflag`, `toggleflag`)

**Happy Path:**
1. User types `setflag 1` (or `clearflag 1` / `toggleflag 1`).
2. The bit number is parsed with `atoi(args)`.
3. `set_flag` / `clear_flag` / XOR is applied to `sys->status_reg`.
4. New hex value of the register is printed.

**Edge Cases & Error Handling:**
- **Out-of-range bit (`setflag 8`, `clearflag -1`):** `bit < 0 || bit > 7` check triggers the usage message. The register is not modified. Note: `atoi` returns 0 for non-numeric input, and `bit < 0` catches negative values; `atoi("abc")` returns 0, which is a valid bit and would set bit 0 silently — a subtle input-validation gap.
- **NULL args (no argument provided):** `args ? atoi(args) : -1` evaluates to `-1`, which fails the `< 0` check, printing the usage message safely.

---

### Flow: Resetting the Simulator (`reset`)

**Happy Path:**
1. User types `reset`.
2. `cmd_reset` sets `log.head`, `log.tail` back to `log.base`, sets `log.count` to 0, zeroes `status_reg` and `tick`.
3. Prints confirmation. The heap allocation itself is **not** freed and reallocated — the buffer data array remains in place, just logically empty.

**Edge Cases & Error Handling:**
- No error paths. The allocated memory is reused — old packet data remains in the buffer slots but is unreachable because `count = 0`. This is correct and deliberate (avoids unnecessary `free`/`malloc`).

---

### Flow: Graceful Shutdown (`quit` / `exit`)

**Happy Path:**
1. User types `quit` or `exit`.
2. `dispatch()` matches the command before scanning the table and returns `0`.
3. `main.c`'s `while(1)` loop breaks.
4. `cb_free(&sys.log)` is called — `free(cb->data)`, then all four pointers nulled and both integers zeroed.
5. `"Simulator shutdown. Memory freed."` is printed. Process exits with code 0.

**Edge Cases & Error Handling:**
- **EOF / stdin closed (e.g., piped input exhausted, Ctrl+D):** `fgets` returns `NULL`. The `if (!fgets(...)) break;` guard exits the loop without calling `dispatch`. `cb_free` is still called. The shutdown message is still printed. Exit code is 0.
- **Signal interruption (Ctrl+C / SIGINT):** Not handled. The process is terminated by the OS immediately. `cb_free` is **not** called. The heap is reclaimed by the OS anyway, but this means a Valgrind run terminated with SIGINT will report a reachable-block leak. The `valgrind` Makefile target (Task 50 in CURRICULUM.md) is designed to avoid this by piping `quit` as the last command.

---

### Flow: Unknown Command

**Happy Path (failure gracefully handled):**
1. User types an unrecognised string (e.g., `foo`).
2. `dispatch()` finds no match in `COMMAND_TABLE[]`.
3. `if (cmd[0] != '\0')` guard prints `Unknown command 'foo'. Type 'help'.`
4. Returns `1` — the simulator continues running.

**Edge Cases:**
- **Blank/whitespace-only input:** `sscanf` on a whitespace-only string leaves `cmd[0] = '\0'`. The guard suppresses the "Unknown command" message — a blank Enter produces no output, just a new prompt. (`dispatch` still returns 1, so the loop continues.)
- **Input longer than `MAX_CMD_LEN - 1` (63 chars):** `fgets` truncates at 63 characters plus a null terminator. The excess bytes remain in `stdin`'s buffer and will be read as the next input line. This can cause garbled multi-line behaviour for very long inputs — an inherent limitation of fixed-size REPL buffers.

---

## 5. Architectural Observations & Constraints

| Property | Detail |
|---|---|
| **Concurrency** | Single-threaded. No mutexes, no async I/O. All state transitions are synchronous and deterministic within a single turn of the REPL loop. |
| **Persistence** | None. All state (log, register, tick) lives in a single `SystemState` stack variable and its heap-allocated sub-buffer. Everything is lost on process exit. |
| **Memory footprint** | Exactly one heap allocation for the lifetime of the process: `BUFFER_SIZE * sizeof(PacketView)` = 128 bytes at default settings. |
| **Extensibility** | Adding a new command requires: (1) a handler function, (2) one row in `COMMAND_TABLE[]`, (3) one line in `cmd_help()`. Adding a new sensor requires: (1) a new `#define` in `defs.h`, (2) two `switch` cases in `sensor.c`, (3) a `case` in `process_packet()`, (4) incrementing the loop bound in `cmd_read()`. |
| **Build modes** | `make all` → production binary `simulator`. `make debug` → `simulator_debug` with `DBG()` traces to `stderr`. `make clean` → removes all binaries and `.o` files. |
| **Portability** | Strictly C11. No POSIX-specific APIs. Should compile cleanly on Linux, macOS, and Windows (MinGW/MSVC with minor adjustments). |
