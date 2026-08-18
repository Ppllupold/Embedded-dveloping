# Computer Architecture — Learning Log

Running notes on CPU/hardware fundamentals, built up during mentoring sessions.
Started: 2026-07-13.

## What a CPU actually does

- A CPU only performs a small set of extremely simple operations (add, subtract, compare, move data, jump) — complexity comes purely from doing these billions of times per second, not from any single operation being sophisticated.
- **Fetch-decode-execute cycle**, repeated forever while the CPU runs:
  1. **Fetch** — read the instruction bytes at the address held in the **Program Counter (PC)**, a special CPU register holding "address of the next instruction to run."
  2. **Decode** — the control circuitry interprets that bit pattern to determine which operation it represents.
  3. **Execute** — actually perform it (ALU does math, a value gets moved/stored, or execution jumps elsewhere).
  4. PC updates — normally to the next sequential address, *unless* the instruction just executed was a jump/branch, in which case PC is set to a different address entirely.
- **A function call is, at the hardware level, just a jump instruction** — it sets the PC to the function's address instead of the next sequential one. This is the literal mechanism behind "the OS jumps to `_start`, which calls `main`" from the very first C-language session (see `c_rules.md`).
- **Registers** (CPU-internal storage, distinct from RAM): a small, fixed number of storage slots wired directly into the CPU's own circuitry — effectively instantaneous to access, unlike RAM which is comparatively slower even though still nanoseconds-fast. The ALU only ever computes on values sitting in registers, never directly on RAM. (Note: "register" is an overloaded term across three unrelated contexts in this track — CPU registers here, the C `register` keyword, and memory-mapped hardware/peripheral "registers" like GPIO/UART config registers. Same word, different concepts.)

## What happens when the PC jumps to invalid/garbage memory

The CPU has no concept of "this doesn't look right" — it always attempts to interpret whatever bits are at the PC's address as *some* instruction. Real outcomes:

1. **Bit pattern matches no valid instruction encoding** — triggers a hardware fault (on ARM Cortex-M: **UsageFault**, possibly escalating to **HardFault**). Loud and catchable, *if* firmware has actually configured a fault handler — nothing does this automatically on bare metal, unlike a desktop OS.
2. **Bit pattern coincidentally matches some real instruction** (just not one anyone intended) — the CPU cannot distinguish "real code" from "garbage that happens to be shaped like code." It executes it anyway: chaotic, unpredictable, often *not* immediately loud. This is the dangerous case.
3. **Address isn't valid/mapped memory at all** — faults even earlier, at the fetch stage (on Cortex-M: **BusFault**).

## Stack smashing: buffer overflow → hijacked control flow

Connects directly to the string buffer-overflow bug from `c_rules.md`:

- When function `A` calls function `B`, the **return address** (where to resume in `A`) is pushed onto the **stack** as part of `B`'s stack frame.
- `B`'s local variables — including local buffers/arrays — also live on that same stack, typically near the saved return address.
- An unchecked buffer write (e.g. `strcpy` overflowing a fixed-size local `char` array) can write past the buffer into that nearby saved return address, corrupting it — `B` keeps running normally, unaware.
- When `B` executes `return`, the CPU loads whatever is *currently* in that stack slot into the PC — the corrupted value, not the original valid address — and jumps there unconditionally (per the fetch-decode-execute behavior above).
- This is the real mechanism behind classic "stack-smashing" exploits: a plain data buffer overflow becomes a full control-flow hijack, no exotic technique required.

## Memory layout — program segments

A running program's address space is divided into distinct segments:

- **`.text` (code):** compiled machine instructions — what the fetch-decode-execute cycle reads. Typically **read-only** (enforced by hardware — MPU on many MCUs, MMU on desktop): prevents a memory-corruption bug from directly overwriting instructions the CPU is about to execute, converting a potential silent code-injection/corruption scenario into an immediate hard fault instead. Related concept: **DEP/NX ("W^X")** — writable memory (stack/heap) is typically also marked non-executable, so even injected bytes there can't be fetched and run as instructions.
- **`.data`:** globals/statics with an explicit non-zero initial value (e.g. `int count = 5;`). The initial value must be stored as real bytes in the compiled binary/flash image.
- **`.bss`** ("Block Started by Symbol"): globals/statics that are uninitialized or explicitly zero (`int count;` — C guarantees globals/statics default to `0`, unlike garbage-valued uninitialized locals). Costs **zero extra bytes** in the compiled file — the compiler just records "reserve N bytes here, zero them at startup," since the guaranteed-zero value needs no literal storage.
- **Heap:** grows *upward* (toward higher addresses) as more is `malloc`'d.
- **Stack:** grows *downward* (toward lower addresses) as function calls nest deeper. Sits at the opposite end of the address space from the heap, growing toward it.

**Embedded-specific detail — flash vs. RAM split:** on a microcontroller, `.text` and `.data`'s *initial values* live in flash (non-volatile). `.data` variables must actually run from RAM (needs to be read/write at runtime). This creates two addresses per `.data` variable: a **load address** (in flash) and a **run address** (in RAM). Startup code (`crt0`/reset handler, running before `main`) does the concrete work: copies `.data` byte-for-byte from its load address in flash to its run address in RAM, and zeroes the entire `.bss` region in RAM (`.bss` needs no copying — it's a pure zero-fill). Only after both steps complete does `main` run. You'll see this exact sequence in real `startup_*.s` files for STM32/similar parts.

**Stack/heap collision risk:** on desktop, the gap between the two (in a huge virtual address space) makes collision rare. On a microcontroller with tight RAM (often 20–64KB total), that gap is small — deep recursion or large local arrays can grow the stack down far enough to collide with a heap grown up far enough, corrupting both. A real, practically-relevant embedded failure mode, not just textbook trivia.

## Alignment and struct padding

- CPUs read memory in fixed-size chunks matching their word size and generally require (or strongly prefer, for performance) multi-byte values to start at an address that's a multiple of their size — a 4-byte `int` wants a 4-byte-aligned address. Misaligned access costs extra cycles on tolerant architectures, or hard-faults on stricter ones.
- **This forces the compiler to insert invisible padding bytes** between struct members so each one starts at a properly aligned offset. Example: `struct { char a; int b; char c; };` — `a` at offset 0, then 3 padding bytes (offsets 1–3) so `b` lands on offset 4 (4-byte aligned), then `c` at offset 8.
- **Trailing padding rule:** the struct's *total* size must also be a multiple of its largest member's alignment requirement (so every element of an array of these structs stays aligned too). For the example above: offset 8 + 1 byte for `c` = 9, not a multiple of 4 → 3 more trailing padding bytes → real `sizeof` = **12**, not the naively-expected `6`. Confirmed live.
- **Reordering members largest-alignment-first reduces (but doesn't always eliminate) padding**: `struct { int b; char a; char c; };` → `b` at 0, `a` at 4, `c` at 5, running total 6, padded up to the next multiple of 4 → **8** bytes. Confirmed live — better than 12, still not a perfect 6, since two 1-byte fields alone can't fully close a 4-byte gap.
- **Real embedded stakes:** a struct meant to overlay a hardware register block, or to parse a raw byte stream matching an external protocol's exact layout, gets silently shifted out of alignment with the real data if padding isn't accounted for — every member after the first mismatch reads/writes the wrong bytes, with no compiler error.
- **Fixes:** reorder members (portable, standard C), or force zero padding with a compiler-specific directive like GCC's `__attribute__((packed))` — at the cost of slower unaligned access and non-portable syntax.

## Toolchain setup — cross-compilation

- **Target board decided: STM32 Nucleo (F401RE or F411RE)** — chosen based on live job-market research: STM32/ARM Cortex-M dominates defense-adjacent embedded postings (43% of STM32 roles are Defense, 29% Aerospace). Nucleo boards have an integrated ST-Link debugger (no separate JTAG probe needed to start) and match the track's existing GPIO/timers/UART/RTOS progression.
- **`arm-none-eabi-gcc` installed and verified working** — official ARM GNU Toolchain (15.3.rel1), Windows `arm-none-eabi.msi` package (not `aarch64-*`, which targets 64-bit ARM/Linux, wrong for Cortex-M).
- **Gotcha hit and resolved:** this installer's default install path has a **literal space**, not a backslash, embedded in the folder name — `C:\Program Files\Arm\GNU Toolchain mingw-w64-x86_64-arm-none-eabi\bin` (one folder named "GNU Toolchain mingw-w64-x86_64-arm-none-eabi" with a space in it, not two nested folders). A PATH entry using a backslash there instead of the actual space silently fails to resolve, with no indication why — always verify the *exact* real folder name via `dir`, don't assume path structure from how it reads visually.

## Linker scripts

- A linker script (`.ld` file) is its own small language telling the linker exactly where in physical memory each segment goes — necessary because bare-metal has no OS loader to decide this dynamically.
- **`MEMORY` block** declares physical regions with real chip numbers, e.g. for STM32F401RE (confirmed via datasheet: 512K flash, 96K RAM): `FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K` / `RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 96K`. These addresses (`0x08000000` flash, `0x20000000` RAM) are the standard ARM Cortex-M/STM32 memory map convention.
- **`SECTIONS` block** places each segment: `.text > FLASH` (code executes directly from flash on most STM32 setups); `.data > RAM AT> FLASH` (the load-address/run-address split from before, now with real syntax — run copy in RAM, load copy physically in FLASH); `.bss > RAM` (no load address at all, pure zero-fill).
- **Wildcard syntax `*(.text*)`**: outer `*` = "from any input object file being linked" (don't have to name every `.o` individually); inner `.text*` (trailing `*`) = "any section name *starting with* `.text`," since compilers often split code into sub-sections (`.text.startup`, or one section per function under `-ffunction-sections`) — without the trailing wildcard, only a section named exactly `.text` would be captured.
- **The location counter `.`**: a linker-script built-in representing "the current output address reached so far" (unrelated to filesystem "current directory" despite the same character) — it auto-advances as content is placed. `_data_start = .;` before placing content, `_data_end = .;` after, gives your startup code linker-computed symbols for the exact size to copy (`_data_end - _data_start`), instead of hardcoding it.

## Why `.data` must run from RAM, not flash (flash vs. RAM as physical technologies)

- **Flash**: non-volatile (retains data with no power — why compiled code/constants live there permanently). Physically: floating-gate transistors trapping electric charge to represent bits, isolated so the charge persists without power. **Reads are fast** (comparable to RAM in many designs; many MCUs execute code directly from flash, "execute in place"). **Writes are fundamentally different and slow**: must first *erase* an entire block/sector (several KB) at once, then *program* individual bits within it — an erase-then-program cycle taking milliseconds, not nanoseconds, and it **wears the physical cells out** (datasheets specify a limited erase/write cycle count per sector, often ~10,000–100,000).
- **RAM (SRAM)**: volatile (loses contents instantly without power), built from cross-coupled transistor latches — fast, arbitrary, byte-level read/write with no wear-out limit.
- **Why this forces the split:** `.data`/`.bss` hold mutable variables, by definition written to constantly during execution. If they lived directly in flash, every write would be a slow erase/program cycle, and anything updated frequently (e.g. a counter incrementing every second) would exhaust flash's write-cycle budget and permanently damage that region within hours. RAM has no such limit — so mutable data must actually run from RAM, with flash only holding its initial values (`.data`'s load address) or nothing at all (`.bss`, pure zero-fill, never read from flash).

## Hardware arrived — driver setup

- Physical Nucleo-F401RE received and connected. ST's own driver download page (STSW-LINK009 / STM32CubeProgrammer, both on `st.com`) failed silently across two browsers with extensions disabled — click "accept license," nothing downloads, no error shown. Root cause not fully diagnosed (site-side issue, not local browser config).
- **Resolved via Zadig** (zadig.akeo.ie, legitimate open-source WinUSB driver installer, standard tool for exactly this class of problem with debug probes like ST-Link/J-Link): Options → List All Devices → select ST-Link interface → assign WinUSB → Install. Board must stay **plugged in** during this (opposite of the usual "install driver before connecting" advice — Zadig needs the device already enumerated to list it).
- Device Manager showed a second unrelated warning-triangle entry, generic "PCI Device" — checked via Properties → Details → Hardware Ids, found `VEN_1022` (AMD, not ST's `VID_0483`) — confirmed unrelated to the Nucleo board (some AMD chipset component), safely ignored.
- Final state confirmed: "ST-Link Debug" clean under USB devices, "STMicroelectronics STLink Virtual COM Port (COM3)" clean under Ports — board fully recognized on both interfaces.

## First real hardware milestone: GPIO blink, verified working

- **Confirmed live on real Nucleo-F401RE hardware: LD2 (PA5) blinking**, driven entirely by hand-written register code — no HAL, no vendor library.
- Register addresses used (STM32F401, verified against reference manual RM0368 and cross-referenced community sources): `RCC_AHB1ENR` = `0x40023830` (bit 0 = `GPIOAEN`), `GPIOA_MODER` = `0x40020000`, `GPIOA_ODR` = `0x40020014`.
- **Peripheral clock gating**: every STM32 peripheral's clock is disabled by default at reset (power saving) — using a peripheral without first enabling its clock in the relevant `RCC_*ENR` register does nothing, silently, no error. Classic first real bug; always step zero for any new peripheral.
- **`startup.c`** implements the crt0 concept for real: a `vector_table[2]` array (entry 0 = raw initial stack-pointer value via `&_estack`, entry 1 = `Reset_Handler` address) placed via `__attribute__((section(".isr_vector")))`; `Reset_Handler` copies `.data` word-by-word from its linker-provided load address (flash) to run address (RAM), zeroes `.bss`, then calls `main()`.
- **`linker_script.ld`** extended with an `.isr_vector` section (`KEEP()`'d so the linker doesn't discard it as unreferenced dead code) pinned first in `FLASH`, plus `_estack` (`ORIGIN(RAM) + LENGTH(RAM)`) and `_data_load_start` (`LOADADDR(.data)`) symbols the startup code reads directly.
- **Cross-compile flags used**: `-mcpu=cortex-m4 -mthumb -mfloat-abi=soft` (target the exact core/instruction encoding; `soft` float-abi since no FP math needed here), `-nostartfiles` (don't let the toolchain link its own hosted-environment startup code — we supply our own), `--specs=nosys.specs` (stub out OS-dependent libc functions like `_exit`/`_sbrk` that have nothing to back them on bare metal), `-T linker_script.ld`.
- **Flashing method**: ST-Link's virtual USB mass-storage drive — drag `blink.bin` (produced via `arm-none-eabi-objcopy -O binary blink.elf blink.bin`) onto it directly, no STM32CubeProgrammer install needed. LD1 (ST-Link status LED) blinking during the copy is normal write-activity indication, not necessarily an error — check for a `FAIL.TXT` on the drive to confirm an actual failure rather than assuming from LED pattern alone.

---
*Computer architecture / memory layout / toolchain / linker script fundamentals complete. First real hardware GPIO project (LED blink) built and verified working end-to-end. Next: timers (replacing the crude busy-wait delay with a real hardware timer), then interrupts.*
