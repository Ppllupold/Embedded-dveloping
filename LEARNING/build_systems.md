# Build Systems — Learning Log

Running notes on Make/build tooling, built up during mentoring sessions.
Started: 2026-07-14.

## Why a build tool at all

- Manually typing one `gcc` command covering every `.c` file gets unwieldy as a project grows, and — the real problem — it recompiles *everything* every time, even files that haven't changed. On a large real project that's minutes wasted per one-line fix instead of seconds.
- A build tool's actual job: track, per file, "is the compiled output already up to date, or does it need rebuilding?" — and only redo the work that's actually necessary.

## Make — rule mechanism

- A `Makefile` rule: `target: dependencies` on one line, then a **Tab-indented** (not space-indented) command line beneath it. Space indentation fails with a cryptic `missing separator` error — a famous, notorious Make gotcha.
- **Timestamp-based rebuild logic**: for a rule `main.o: main.c`, Make checks whether `main.o` exists and whether it's newer than `main.c`. If `main.o` is missing or older than its dependency, the target is "out of date" and the recipe runs. If it's already newer than every dependency, Make skips the recipe entirely — no rebuild.
- **Verified live** on a 2-file project (`main.c` + `math_utils.c`/`.h`): editing only `math_utils.c` caused Make to recompile *only* `math_utils.o`; `main.o` was left untouched since neither `main.c` nor `math_utils.h` had changed. The final `program` link step still re-ran, because one of its two dependencies (`math_utils.o`) got a new timestamp — the link target depends on both object files, so any one of them changing is enough to trigger relinking.
- **Make's own variable syntax** (a separate mini-language, unrelated to C or the shell): `CC = gcc` defines a variable; `$(CC)` elsewhere expands to its value. `$(CC) $(CFLAGS) -c main.c -o main.o` expands to the literal shell command at run time.
- `-c` flag (previously covered as "not what you want for running a program directly") is exactly the right tool for a Make rule building an object file: compile-only, no linking, producing the `.o` that gets combined later at the single final link step.

## Related concepts clarified alongside this

- **`.h` files**: headers — declarations only (prototypes, types, macros), shared across `.c` files via `#include`.
- **`.o` files**: object files — one source file's compiled machine code, pre-linking. Direct, now-visible instance of the compiler pipeline's middle stage (preprocessor → compiler → object file → linker) from the very first session.
- **`#` lines** (`#include`, `#ifndef`, `#define`, `#endif`) are preprocessor directives, not C syntax — handled entirely by the preprocessor stage, before the real C compiler ever runs.
- **Header guards** (`#ifndef X_H` / `#define X_H` / `#endif`): prevent a header's contents from being pasted in twice if included multiple times (directly or via an include chain) — directly solves the circular-include concern raised earlier.

## Pattern rules and automatic variables

- **`%.o: %.c`** — a pattern rule: `%` is a wildcard, so one rule stands in for "build any `X.o` from `X.c`," replacing a separate explicit rule per source file. Explicit rules still take precedence over a pattern rule for the same target if both exist — a leftover explicit rule will silently "win" and stop the pattern rule from applying to that one file.
- **Automatic variables**: `$@` = the current rule's target; `$<` = the *first* dependency only; `$^` = *all* dependencies (space-separated).
  - Compile rules want `$<` (only the `.c` file should become a compiler argument — header dependencies are listed purely for rebuild-tracking, never meant to be passed to the compiler).
  - Link rules want `$^` (every object file needs to be passed to the linker): `program: main.o math_utils.o` + recipe `$(CC) $(CFLAGS) $^ -o $@`.
- Compiling generalizes fully into one pattern rule; **linking does not** — different shape (many inputs → one output vs. one-to-one), needs its own dedicated rule. The *list* of object files to link still has to be maintained by hand in plain Make.

## Auto-dependency generation (`-MMD -MP`)

- Solves the "manually forgot to list a header dependency" risk: compiling with `-MMD -MP` makes GCC emit a `.d` file per source file (e.g. `main.d`), containing a Make rule *it generated itself* by actually scanning the real `#include` chain (e.g. `main.o: main.c math_utils.h`).
- `-include $(wildcard *.d)` in the Makefile pulls all generated `.d` files in automatically; leading `-` means "don't error if none exist yet" (true on the very first build).
- **Verified live:** editing only `math_utils.h` (no change to any `.c` file) correctly triggered a rebuild of both `main.o` and `math_utils.o`, despite the Makefile containing zero manually-written header dependencies anywhere — the auto-generated `.d` files did the tracking correctly on their own.

## CMake

- A **build system generator**, not a build system itself — doesn't compile anything directly. You describe the project once in `CMakeLists.txt` (CMake's own language); CMake generates the actual build files for whatever backend (Makefile, Ninja, Visual Studio project) is appropriate for the target platform. Solves build-tool-syntax portability, not runtime-environment portability (different problem from Docker — Docker packages/ships a consistent runtime environment; CMake generates correct build tooling but still assumes the right compiler/toolchain is already installed).
- Relevant to the track directly: several major embedded SDKs (Raspberry Pi Pico SDK, ESP-IDF for ESP32) are CMake-based by default.
- Minimal example, replacing the entire hand-written Makefile:
  ```cmake
  cmake_minimum_required(VERSION 3.10)
  project(MathDemo C)
  add_executable(program main.c math_utils.c)
  ```
  `add_executable` takes source `.c` files directly (not `.o`s) — it's a higher level of abstraction than Make's explicit compile-then-link rules; CMake decides and generates the full compile/link/dependency-tracking pipeline internally, you only declare the desired input sources and output executable name.
- Usage: `cmake -S . -B build` (generate build files into a fresh `build/` directory, keeping generated files out of the source tree) then `cmake --build build` (invoke the actual underlying build). Resulting binary lands inside `build/` (e.g. `build/program.exe` on Windows).
- Not part of the earlier MSYS2 toolchain install — installed separately via `pacman -S mingw-w64-ucrt-x86_64-cmake` (lands in the already-PATH'd `ucrt64/bin`, no new PATH entry needed).
- Verified live: built and ran the same 2-file project via CMake, matching the Make-built output exactly.

## Linker script syntax — `.` and `*`

Two unrelated symbols, both used constantly in `linker_script.ld`:

- **`.` — the location counter.** A running "current address" the linker tracks while laying out memory. It starts at a region's `ORIGIN` and auto-advances as content is placed. Reading it (`_data_start = .;`) captures a snapshot of "the address we're at right now" into a named symbol — used to bracket a section's real start/end without hardcoding numbers:
  ```ld
  .data : {
      _data_start = .;   /* snapshot: address before .data content is placed */
      *(.data*)          /* . advances as bytes are placed here */
      _data_end = .;     /* snapshot: address after */
  } > RAM AT> FLASH
  ```
  Contrast with `_estack = ORIGIN(RAM) + LENGTH(RAM);` — that's computed directly from the region's known size, not from `.`, since nothing has been placed yet at that point in the script.

- **`*` — wildcard for input files/sections.** Pulls matching content from the compiled `.o` files being linked. In `*(.text*)`: the `*` outside the parens means "from any input object file"; `.text*` inside means "any section whose name starts with `.text`" (compilers sometimes split code into per-function subsections like `.text.main`, so the trailing `*` catches those too). `KEEP(*(.isr_vector))` is the same pattern, wrapped in `KEEP()` so the linker doesn't discard it as unreferenced dead code (nothing calls into `vector_table` directly, so without `KEEP` the linker would assume it's unused).

  Short version: `.` tracks *where* the linker currently is; `*` selects *what* gets pulled in from where.

---
*Build systems section complete. Next up: cross-compilation toolchains (`arm-none-eabi-gcc`) and linker scripts — the direct bridge into real hardware work.*
