# Learning Curriculum: Micro-Kernel Environmental Simulator

> A progressive, 50-task curriculum tied directly to this codebase.
> Go in order. Do not skip phases. Do not look up solutions until you've made a genuine attempt.

---

## Phase 1: Exploration & Tracing (Tasks 1–10)

Read code, add comments, insert `printf` debug lines, and trace data flow. The goal is to build a mental model of how data moves through the system before touching any logic.

---

**Task 1**
Open [defs.h](defs.h). Read every `#define`. For each of the five `FLAG_*` constants (`FLAG_HEATER_ON`, `FLAG_FAN_ON`, `FLAG_SENSOR_ERROR`, `FLAG_LOG_FULL`, `FLAG_NIGHT_MODE`), write a comment above it explaining in plain English what bit position it occupies (0 through 4) and what `1u << N` actually means in binary. Do this by hand — convert each value to its 8-bit binary representation.

---

**Task 2**
Open [sensor.h](sensor.h). Study the `SensorPacket` struct and the `PacketView` union. Add a block comment above each that explains: (a) the size of each field in bytes, (b) the total size of the struct, and (c) why the union's `raw[8]` array is exactly 8 bytes long. Use `sizeof` in your head, not a calculator.

---

**Task 3**
Open [main.c](main.c). Trace the startup sequence line by line. Add an inline comment on every line in `main()` that does something non-trivial (seeding random, initializing `SystemState`, calling `cb_init`, printing the banner, the REPL loop, and `cb_free`). Each comment should say *why* that line exists, not just *what* it does.

---

**Task 4**
Open [buffer.c](buffer.c) and study `cb_init()`. Then add a `printf` statement inside `cb_init()` — after the `malloc` call — that prints the address of the allocated memory block and the total byte size allocated. Build with `make` and run `./simulator`, type any command, and observe what gets printed. Remove the `printf` after you understand it.

---

**Task 5**
Open [sensor.c](sensor.c) and read `fake_sensor()`. Add a `printf` inside each `case` branch that prints the sensor name and the raw value returned by `rand()` before the float conversion. Build and run `./simulator`, type `read`, and observe the raw random numbers before they become sensor readings. Remove the `printf` statements after you understand them.

---

**Task 6**
Open [commands.c](commands.c) and read `process_packet()` (lines 15–44). Add a `printf` at the very start of the function that prints the sensor ID and the float value of every packet that enters it. Then type `read` several times in the running simulator and watch every packet flow through. Remove the `printf` after you understand it.

---

**Task 7**
Open [buffer.c](buffer.c) and study `cb_next()`. On paper, draw a diagram of the `CircularBuffer` with `capacity = 4`. Mark the `base`, `head`, and `tail` pointers. Then manually trace what happens to each pointer when you call `cb_push` four times (filling the buffer) and then a fifth time (overwriting the oldest). Write your diagram as a comment block above `cb_next()`.

---

**Task 8**
Open [commands.c](commands.c) and read `dispatch()` (lines 238–259). Trace exactly what happens when you type `inject 0 99.5` at the prompt: which string gets parsed into which variable, how the `COMMAND_TABLE` is searched, and which function pointer gets called. Add a comment above the `for` loop in `dispatch()` that explains the search strategy in one sentence.

---

**Task 9**
Open [status.c](status.c) and read `print_status_reg()`. Manually compute what the output would be for the value `0b00010110` (decimal 22). Write out the hex representation, the binary representation bit by bit, and which named flags would be shown as ON. Then verify your answer by running `./simulator`, typing `setflag 1`, `setflag 2`, `setflag 4`, and then `status`.

---

**Task 10**
Open [commands.c](commands.c) and read `cmd_dump()` (lines 93–118) carefully, focusing on the pointer arithmetic on line 116. Add a `printf` inside the dump loop that also prints the raw memory address of the `PacketView` being printed at each iteration. Run `./simulator`, type `read` three times, then `dump`. Observe how the addresses relate to `base`, confirm the wrap-around logic is correct. Remove the `printf` after you understand it.

---

## Phase 2: Minor Modifications (Tasks 11–25)

Safely tweak existing features, change thresholds, alter output formatting, and extend existing logic without breaking anything.

---

**Task 11**
Open [defs.h](defs.h). Change `BUFFER_SIZE` from `16` to `8`. Rebuild with `make`. Run `./simulator` and type `read` nine times. Observe when `FLAG_LOG_FULL` gets set in `status` output. Then change it to `4` and repeat. Restore `BUFFER_SIZE` to `16` when done.

---

**Task 12**
Open [defs.h](defs.h). Change `TEMP_HIGH_THRESH` from `30.0f` to `25.0f`. Rebuild. Run `./simulator` and type `read` repeatedly until a temperature above 25°C is sampled. Verify the fan turns on sooner. Understand the relationship between this constant and the logic in [commands.c](commands.c) lines 20–22. Restore the original value.

---

**Task 13**
Open [sensor.c](sensor.c) and read `sensor_name()` and `sensor_unit()`. Add a fourth sensor case to both functions: sensor ID `3` should return the name `"Pressure"` and the unit `"hPa"`. Do not break the existing three cases. Do not add a sensor reading for it yet — just make both lookup functions aware of it.

---

**Task 14**
Open [sensor.c](sensor.c) and read `fake_sensor()`. Add a `case 3` to `fake_sensor()` that generates a realistic atmospheric pressure reading between 950.0f and 1050.0f hPa. Use the same `rand()` pattern as the existing cases. This extends the work from Task 13.

---

**Task 15**
Open [commands.c](commands.c) and read `cmd_read()` (lines 50–63). It currently loops over sensors 0, 1, and 2. Change the loop bound so it also reads sensor ID 3 (the Pressure sensor you added in Tasks 13–14). Rebuild and run `./simulator`. Type `read` and confirm all four sensors are sampled and logged.

---

**Task 16**
Open [commands.c](commands.c) and read `cmd_help()` (lines 200–216). The help text for `inject` reads `inject <id> <val>`. Update this description to also mention valid sensor IDs (`0=Temp, 1=Humidity, 2=Light, 3=Pressure`). Keep the formatting consistent with the other help lines.

---

**Task 17**
Open [status.c](status.c) and read `print_status_reg()` (lines 9–20). Currently it prints unnamed flags (bits 5–7) without a label. Modify the function so that if an unnamed bit is set, it prints something like `[BIT5: ON]` instead of leaving it unlabeled. Test this by running `./simulator` and typing `setflag 5`, then `status`.

---

**Task 18**
Open [commands.c](commands.c) and read `cmd_inject()` (lines 65–82). Currently it silently does nothing if you pass an invalid sensor ID (e.g., `inject 9 50.0`). Add a bounds check: if `sensor_id` is less than 0 or greater than 3 (after your Task 13–15 changes), print an error message like `Error: unknown sensor ID` and return early without pushing to the log.

---

**Task 19**
Open [commands.c](commands.c) and read `cmd_status()` (lines 84–91). Add one more line of output that prints the memory address of the `CircularBuffer`'s `data` pointer (i.e., `sys->log.data`). This shows the heap address of the log. Format it with `%p`. Rebuild and confirm the address stays the same across commands but changes after a `reset`.

---

**Task 20**
Open [defs.h](defs.h). Add a new threshold constant `PRESSURE_LOW_THRESH` with value `970.0f`. Then open [commands.c](commands.c) and add a block inside `process_packet()` that handles sensor ID 3: if pressure falls below `PRESSURE_LOW_THRESH`, set `FLAG_SENSOR_ERROR` (treat low pressure as a sensor anomaly). Follow the exact same pattern as the temperature and humidity blocks above it.

---

**Task 21**
Open [commands.c](commands.c) and read `cmd_dump()`. Currently it prints sensor name, value, unit, and timestamp. Add the sensor ID number to the output so each line shows, e.g., `[ID:0] Temperature: 27.34 C  ts=142`. This requires printing `pkt.sensor_id` alongside the existing fields.

---

**Task 22**
Open [main.c](main.c) and look at the REPL loop (lines 37–48). Currently if you type only whitespace or an empty line, `dispatch()` is called with an empty/whitespace string. Modify the loop to skip `dispatch()` entirely when the trimmed input is empty. You will need to use `strlen()` or check for `'\n'` directly.

---

**Task 23**
Open [commands.c](commands.c) and read `cmd_reset()` (lines 189–198). Currently it clears the log and zeros `status_reg`, but it does not reset `sys->tick`. Modify `cmd_reset()` so it also resets `tick` to 0. Then run `./simulator`, type `read` a few times to advance the tick, type `reset`, then `read` again, and verify the timestamp on the new readings starts from a low number again.

---

**Task 24**
Open [commands.c](commands.c) and read `cmd_setflag()` and `cmd_clearflag()` (lines 165–179). Both functions take a bit number `N` but do no range checking. Add validation to both: if `N` is less than 0 or greater than 7, print `Error: bit must be 0-7` and return without modifying the register.

---

**Task 25**
Open [commands.c](commands.c) and read `cmd_rawbytes()` (lines 120–148). It currently only displays the raw bytes of the last entry in the log. Modify it to also print the total count of entries currently in the log (`sys->log.count`) at the top of its output, before the byte display. Format it as `Log entries: N`.

---

## Phase 3: New Micro-Features (Tasks 26–40)

Add small, isolated features that follow the exact patterns already established in the codebase.

---

**Task 26**
Open [commands.c](commands.c) and study the `cmd_pop()` handler (lines 150–163) and its entry in `COMMAND_TABLE` (around line 230). Add a new command `peek` that shows the oldest log entry *without* removing it — like `pop` but non-destructive. Follow the exact same pattern: write a `cmd_peek()` function and add it to `COMMAND_TABLE`. Update `cmd_help()` to include it.

---

**Task 27**
Open [commands.c](commands.c) and study `cmd_status()` (lines 84–91). Add a new command `count` that simply prints the number of entries currently in `sys->log.count`. It requires a new `cmd_count()` function, a new `COMMAND_TABLE` entry, and a `cmd_help()` entry. Keep the implementation to 3–4 lines.

---

**Task 28**
Open [defs.h](defs.h) and add a new flag constant `FLAG_ALARM` defined as `1u << 5`. Then open [commands.c](commands.c) and modify `process_packet()` to set `FLAG_ALARM` whenever *both* `FLAG_HEATER_ON` and `FLAG_SENSOR_ERROR` are set at the same time — a simulated alarm condition. Use `test_flag()` from [status.h](status.h) to check both.

---

**Task 29**
Open [commands.c](commands.c) and study `cmd_inject()`. Add a new command `injectraw` that accepts three integers: `<sensor_id> <timestamp> <int_value>`. It should call `make_packet()` directly with the exact timestamp the user provides (instead of using `sys->tick`). This gives you manual control over the timestamp for testing. Follow the `cmd_inject()` pattern exactly.

---

**Task 30**
Open [commands.c](commands.c) and study `cmd_dump()`. Add a new command `dumptemp` that behaves like `dump` but only prints entries where `pkt.sensor_id == SENSOR_TEMP`. All other entries are silently skipped. Use the same traversal loop as `cmd_dump()` — do not write a new traversal mechanism.

---

**Task 31**
Open [commands.c](commands.c) and study `cmd_setflag()` (lines 165–171). Add a new command `flaginfo` with no arguments. It should print the name and current state (ON/OFF) of all five named flags (`FLAG_HEATER_ON` through `FLAG_NIGHT_MODE`). Use `test_flag()` from [status.h](status.h) for each check. Follow the output style of `print_status_reg()` in [status.c](status.c).

---

**Task 32**
Open [buffer.h](buffer.h) and [buffer.c](buffer.c). Add a new function `cb_is_full(const CircularBuffer *cb)` that returns `1` if the buffer is full and `0` otherwise. Declare it in the header and implement it in the source. Then find every place in [commands.c](commands.c) where `sys->log.count == sys->log.capacity` is checked manually and replace those expressions with a call to your new function.

---

**Task 33**
Open [sensor.c](sensor.c) and study `fake_sensor()`. Add a new exported function `clamp_value(float value, float min, float max)` that returns `value` clamped to the `[min, max]` range. Declare it in [sensor.h](sensor.h). Then use it inside `fake_sensor()` on the return value of each case, clamping temperature to `[-10.0f, 50.0f]`, humidity to `[0.0f, 100.0f]`, and light to `[0.0f, 1000.0f]`.

---

**Task 34**
Open [commands.c](commands.c) and study `cmd_read()`. Add a new command `readone <id>` that reads only a single specified sensor (by ID), logs it, and processes it — instead of reading all sensors. Reuse `fake_sensor()`, `make_packet()`, `cb_push()`, and `process_packet()` exactly as `cmd_read()` does.

---

**Task 35**
Open [commands.c](commands.c) and study `cmd_dump()`. Add a new command `last` that prints only the most recently pushed entry in the log (not the oldest — the newest). This requires computing a pointer to the entry just before `head` with correct wrap-around using pointer arithmetic and `cb_next()` or equivalent logic. Do not use `cb_pop()`.

---

**Task 36**
Open [defs.h](defs.h) and add a new constant `MAX_LOG_HISTORY 3`. Then open [commands.c](commands.c) and add a new command `recent` that prints only the last `MAX_LOG_HISTORY` entries from the log (most recent first). If the log has fewer than `MAX_LOG_HISTORY` entries, print whatever is available. This will require careful reverse traversal using pointer arithmetic.

---

**Task 37**
Open [status.c](status.c) and [status.h](status.h). Add a new function `count_set_flags(unsigned char reg)` that returns the number of bits that are currently set to 1 in the register. Implement it using a loop from bit 0 to bit 7. Declare it in the header. Then use it inside `cmd_status()` in [commands.c](commands.c) to print a line like `Active flags: 2`.

---

**Task 38**
Open [commands.c](commands.c) and study `cmd_dump()`. Add a new command `avg <id>` that iterates over all log entries, filters by the given sensor ID, and computes and prints the average value of those readings. If no entries exist for that sensor ID, print `No data for sensor <id>`. Use the same traversal loop as `cmd_dump()`.

---

**Task 39**
Open [main.c](main.c) and look at the REPL banner (lines 24–33). Add a new line to the startup banner that prints the current value of `BUFFER_SIZE` and the size in bytes of one `PacketView` entry (use `sizeof(PacketView)`), formatted as: `Log capacity: 16 slots x 8 bytes = 128 bytes`. Make the numbers dynamic using the actual constants and `sizeof` — no hardcoded numbers.

---

**Task 40**
Open [commands.c](commands.c) and [commands.h](commands.h). Add a new command `save` that takes a filename argument (e.g., `save log.txt`) and writes the entire `dump` output to that file instead of stdout. Use `fopen()`, `fprintf()`, and `fclose()`. Reuse the traversal logic from `cmd_dump()`. Print a confirmation like `Saved 5 entries to log.txt` on success, or an error message on `fopen` failure.

---

## Phase 4: Debugging, Refactoring & Testing (Tasks 41–50)

Identify edge cases, extract helper functions, and write manual test harnesses.

---

**Task 41**
Open [buffer.c](buffer.c) and read `cb_init()`. Identify the one error condition that is currently not handled: `malloc()` can return `NULL`. Modify `cb_init()` to check the return value of `malloc()`. If it is `NULL`, print an error to `stderr` and return a non-zero error code (the function currently always returns 0). Then open [main.c](main.c) and make it check this return value and exit gracefully on failure.

---

**Task 42**
Open [commands.c](commands.c) and read `cmd_inject()` (lines 65–82). It uses `sscanf` to parse user input. Identify the edge case where the user types `inject` with no arguments at all (empty `args`). Trace what `sscanf` returns in that case. Add a check: if `sscanf` does not successfully parse exactly 2 values, print `Usage: inject <sensor_id> <value>` and return early.

---

**Task 43**
Open [commands.c](commands.c) and read `cmd_dump()` (lines 93–118). The function has a subtle edge case: what happens when `sys->log.count == 0`? Trace through the loop manually with an empty buffer. Confirm whether it handles this correctly already or not. Then add an explicit early-return guard at the top of the function that prints `Log is empty.` and returns if count is 0, regardless of current behavior.

---

**Task 44**
Open [commands.c](commands.c) and look at `cmd_rawbytes()` (lines 120–148). It computes a pointer to the last pushed entry using manual pointer arithmetic. Extract this pointer calculation into a standalone helper function `get_last_entry(const CircularBuffer *cb)` that returns a `const PacketView *` (or `NULL` if empty). Place it above `cmd_rawbytes()`. Then replace the inline calculation in `cmd_rawbytes()` with a call to your new helper. Check if `cmd_dump()` or `cmd_peek()` (Task 26) could also benefit from it.

---

**Task 45**
Open [sensor.c](sensor.c) and read `make_packet()`. Write a standalone test program `test_packet.c` in the project root that includes [sensor.h](sensor.h) and calls `make_packet()` with known inputs (e.g., `sensor_id=1`, `value=55.5f`, `timestamp=100`). Print every field of the resulting `SensorPacket` and every byte of the `raw[8]` array. Add a `test` target to the [Makefile](Makefile) that compiles and runs this test. The output should be deterministic and easy to verify by eye.

---

**Task 46**
Open [buffer.c](buffer.c) and write a standalone test program `test_buffer.c` that exercises the circular buffer in isolation. It should: (1) call `cb_init` with capacity 4, (2) push 4 packets, (3) verify `count == 4`, (4) push a 5th packet and verify `count` is still 4 (oldest was overwritten), (5) call `cb_pop` 4 times and verify each call succeeds, (6) call `cb_pop` a 5th time and verify it returns `-1`. Print PASS or FAIL for each step. Add a `test_buffer` target to the [Makefile](Makefile).

---

**Task 47**
Open [status.c](status.c). Write a standalone test program `test_status.c` that tests all three flag operations. For each of the five `FLAG_*` constants: (1) start with a zeroed register, (2) call `set_flag` and verify `test_flag` returns 1, (3) call `clear_flag` and verify `test_flag` returns 0, (4) verify no other bits were changed by checking the full register value. Print PASS or FAIL per test. Add a `test_status` target to the [Makefile](Makefile).

---

**Task 48**
Open [commands.c](commands.c) and read `process_packet()` (lines 15–44). Identify all threshold boundary conditions: exactly at `TEMP_HIGH_THRESH` (30.0f), exactly at `TEMP_LOW_THRESH` (15.0f), exactly at `HUMIDITY_HIGH_THRESH` (70.0f), and exactly at `LIGHT_LOW_THRESH` (200.0f). For each one, determine whether the boundary value itself triggers the flag (is the comparison `>` or `>=`?). Write a comment block above `process_packet()` documenting each boundary behavior explicitly.

---

**Task 49**
Open [commands.c](commands.c) and identify all places where a `char *` returned by `sensor_name()` or `sensor_unit()` could be `NULL` (e.g., if an invalid sensor ID is passed). Trace what would happen if `printf` received a `NULL` string for `%s`. Add a defensive helper function `safe_str(const char *s)` in [sensor.c](sensor.c) that returns `"unknown"` if `s` is `NULL`, otherwise returns `s`. Declare it in [sensor.h](sensor.h) and replace all `sensor_name()` / `sensor_unit()` usages in `cmd_dump()` and `cmd_rawbytes()` with `safe_str(sensor_name(...))`.

---

**Task 50**
Open the [Makefile](Makefile). Add a `valgrind` target that runs `./simulator` with a pre-scripted set of commands piped through stdin (use a heredoc or an input file `test_input.txt`) under `valgrind --leak-check=full`. The script should exercise: `read` (5 times), `inject 0 5.0`, `dump`, `pop`, `reset`, `read`, `rawbytes`, and `quit`. Confirm that valgrind reports zero memory leaks and zero errors. If it does not, find and fix the leak.

---

## Reference: Key File Locations

| Concept | File | Approximate Lines |
|---|---|---|
| Constants, flags, thresholds | [defs.h](defs.h) | 1–30 |
| SensorPacket struct, PacketView union | [sensor.h](sensor.h) | 12–26 |
| Sensor utilities | [sensor.c](sensor.c) | 1–44 |
| CircularBuffer struct | [buffer.h](buffer.h) | 6–13 |
| Buffer alloc/free/push/pop | [buffer.c](buffer.c) | 1–58 |
| Status register operations | [status.c](status.c) / [status.h](status.h) | 1–20 |
| SystemState struct | [system.h](system.h) | 7–11 |
| Command struct, function pointer | [commands.h](commands.h) | 6–9 |
| All command handlers + dispatch | [commands.c](commands.c) | 1–259 |
| Entry point, REPL loop | [main.c](main.c) | 1–54 |
| Build rules | [Makefile](Makefile) | 1–21 |
