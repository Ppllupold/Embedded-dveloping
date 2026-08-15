# C Programming Rules — Learning Log

Running notes on C rules, gotchas, and reasoning, built up during mentoring sessions.
Started: 2026-07-10.

## Program structure

- **`int main(void)`, not `int main()`.** Empty parens `()` in C mean "unspecified arguments" (a leftover from pre-ANSI K&R C) — the compiler won't check call sites for wrong argument counts. `(void)` explicitly declares zero parameters and *is* enforced by the compiler. MISRA C requires `(void)` for exactly this reason: an unchecked call with the wrong arguments can silently corrupt the stack on embedded targets with no OS to catch it.
- **`main` returns `int`**, used by the OS as the process exit code (by convention: `0` = success, nonzero = specific failure). Most shells only preserve the low 8 bits (0–255) of it.
- **Exception:** if control falls off the end of `main` with no `return`, C99+ treats it as `return 0;` — but this special case applies *only* to `main`. Every other non-void function must return explicitly, or it's undefined behavior. Write `return 0;` explicitly anyway — no cost, removes the "which function is this safe on" question.
- Compiling one `.c` file is actually a **pipeline**: preprocessor (text substitution: `#include`, `#define`) → compiler (source → object file `.o`, per file, in isolation) → linker (stitches object files + libraries into one executable, resolves cross-file references, sets the entry point). The OS jumps to a small runtime startup routine (`_start`/`crt0`), which calls `main` as an ordinary function call.

## Compiler hygiene

- Always compile with `-Wall -Wextra -std=c17` (or whichever standard version is intended). Warnings catch a large fraction of real bugs for free — uninitialized variables, signed/unsigned mismatches, unused values, etc.
- `error` vs `warning`: an error means the compiler cannot generate valid code at all (e.g. undeclared identifier — no type, no value, no address to generate an instruction for). A warning means the compiler *can* generate code but the construct is suspicious.
- Editing a `.c` file does **not** update the compiled binary. Must re-run `gcc` after every change — a stale binary is a classic source of "I fixed it but it's still broken" confusion.
## Integer types: signed vs unsigned

- Same bit width, different interpretation of the bit pattern. `int` is signed by default; `unsigned int` (or `unsigned`) has no sign bit, doubling the positive range instead.
- **Signed integer overflow is undefined behavior.** Not guaranteed to wrap — the compiler is legally allowed to assume it never happens, which can lead to optimized-away bounds checks in real, shipped code.
- **Unsigned integer overflow is well-defined**: wraps modulo 2^N (e.g. `unsigned char`: `255 + 1 == 0`). This is the opposite of what intuition suggests — the "simpler" type is the one with guaranteed behavior.
- Classic bug: `unsigned int i = size; for (...; i >= 0; i--)` never terminates, because when `i` hits `0` and decrements, it wraps to the type's max value (e.g. `4294967295` for 32-bit), which still satisfies `i >= 0`.
- **Idiom to avoid it:** decrement before use, check against a strictly-positive lower bound instead of comparing an unsigned value to `0` from below:
  ```c
  unsigned int i = size;
  while (i >= 1) {
      i--;
      // process array[i]
  }
  ```

## Memory safety

- C performs **no runtime array bounds checking**. `array[size]` on a `size`-element array (valid indices `0..size-1`) does not throw — it silently reads/writes `base_address + index * sizeof(element)`, whatever is actually there. On hardware without an MPU, this can corrupt arbitrary memory rather than crash predictably. Bounds discipline is entirely the programmer's responsibility.

## Fixed-width integer types (`<stdint.h>`)

- **Plain `int`/`short`/`long` have no fixed size in C** — the standard only guarantees *minimum* ranges (`int` is at least 16 bits). Actual size is platform/compiler-dependent (often 32-bit on modern platforms, but genuinely 16-bit on some microcontrollers/DSPs). This is unlike Java, where `int` is always exactly 4 bytes by spec.
- **`uint8_t`, `uint16_t`, `uint32_t` (from `<stdint.h>`) guarantee exact, portable size** — `uint32_t` is always exactly 32 bits on every platform that defines it. They're `typedef`s aliasing whatever real type happens to be that width on the target. Use these (not plain `int`) when the width must match real hardware — register widths, tick counters, protocol fields.
- **`printf` and small types:** small integer types passed to variadic functions (`printf`) undergo default argument promotion to `int`/`unsigned int` before the call — so `%u` works fine for a `uint8_t` argument in practice. The fully portable/pedantic option is the `PRIu8`/`PRIu32` macros from `<inttypes.h>`, used like `printf("%" PRIu8 "\n", val);` — needed because `uint8_t` isn't guaranteed to literally be `unsigned char` everywhere.

## Pointers

- A pointer is a variable whose stored value is a memory address, not a "special reference type." `&x` gives the address of `x`; `*p` (outside a declaration) dereferences `p` — reads/writes the value at the address it holds.
- C is always strictly pass-by-value (like Java) — pointers are just how you make an address itself the value being passed/copied, so a function can reach back and modify the caller's data.
- Declaration vs use: `int *p = &x;` declares `p` as "a variable such that `*p` is an `int`," and initializes it to `x`'s address. Later, `*p = 10;` does **not** change what `p` points to — it changes the value stored at the address `p` already holds (i.e. changes `x`). `p = &y;` (no `*`) is what changes what `p` points to.
- **Uninitialized pointers hold garbage** — an arbitrary leftover bit pattern, not `NULL`, not zero, not anything meaningful. Dereferencing a garbage pointer is undefined behavior: on desktop (with an MMU) this often segfaults immediately (the "lucky" case — loud and obvious); on embedded hardware with no MPU, it can silently corrupt an unrelated variable, the stack, or even a memory-mapped hardware register.
- **`NULL` is a defined sentinel value**, not a safe thing to dereference — dereferencing `*p` when `p == NULL` is still undefined behavior (often a reliable crash on desktop, not guaranteed elsewhere). Its purpose is to be a checkable "points nowhere yet" state: initialize pointers to `NULL` when there's no valid target yet, and always gate dereferences with `if (p != NULL) { ... }`.

## Arrays and pointer decay

- `array[i]` is defined by the standard as exactly `*(array + i)` — not "similar to," literally the same thing. (Fun/useless fact: `array[i] == i[array]` for this reason, since addition is commutative.)
- **Pointer arithmetic is scaled by element type size.** `array + 1` on an `int *` (4-byte int) advances the address by 4 bytes, not 1 — the compiler multiplies by `sizeof(element type)` automatically. This is why indexing works correctly regardless of element size.
- **Array decay:** in most expression contexts, an array's name is implicitly converted ("decays") into a pointer to its first element. C arrays are not runtime objects (unlike Java) — no hidden length, no bounds checking, no object overhead.
- **`sizeof` is an exception to decay**: `sizeof(array)` *in the scope where it's declared* gives the true total size (`num_elements * sizeof(element)`), because the compiler still knows the real array type there.
- **Function parameters are the trap:** C cannot pass a whole array by value as an argument. `void foo(int arr[10])` is silently rewritten by the compiler to `void foo(int *arr)` — the `[10]` is ignored. Inside `foo`, `arr` is genuinely just a pointer from then on: `sizeof(arr)` gives the pointer's size (e.g. 8 bytes), not the original array's size. The classic resulting bug: `sizeof(arr) / sizeof(arr[0])` inside such a function silently computes the wrong (small, plausible-looking) element count instead of erroring — always pass array length as an explicit separate parameter (`void foo(int *arr, size_t length)`). GCC's `-Wsizeof-pointer-div` (part of `-Wall` since GCC 8) catches this specific pattern.

## Strings

- C has no real string type — a "string" is just a `char` array with a convention layered on top: it must end with a **null terminator**, `'\0'` (byte value `0`). `"hello"` occupies 6 bytes in memory (5 characters + terminator), not 5.
- This is a pure convention, not compiler-enforced — every string function (`strlen`, `printf`'s `%s`, `strcpy`, ...) agrees to read bytes until it hits `'\0'`, then stop. `NULL` (the pointer sentinel) and `'\0'` (the string-ending byte) are different concepts that share the word "null" — don't conflate them.
- `strlen` has no stored length to look up — it's O(n), walking memory byte-by-byte until it finds the terminator. Unlike Java's `.length()`, this is real work, not a field access.
- **Buffer overflow risk:** if the destination buffer isn't sized to hold the string *plus* its terminator, string-copying functions like `strcpy` will happily write past the end of it — no bounds checking. Compile-time string-literal initialization (`char name[5] = "hello";`) is one of the few cases GCC *can* catch (hard error, sizes known statically). Runtime copies (`strcpy(name, some_runtime_string)`) generally are **not** caught by the compiler, since the source length isn't known until the program runs — this is the realistic, dangerous case, and the root cause of a huge class of historical real-world security vulnerabilities.
- **Ensuring the destination buffer is big enough is entirely the programmer's responsibility** — the language and compiler do nothing for you here by default.

## Function declarations, headers, and the compiler's single pass

- GCC compiles a file top-to-bottom in one pass. Calling a function before the compiler has seen its definition *or* a prior declaration triggers "implicit declaration" — same root cause as calling `printf` without `#include <stdio.h>` earlier. The compiler guesses a signature instead of refusing to compile.
- Two valid fixes: (1) place the full function definition above every call site (fine for small single-file programs), or (2) add a **forward declaration/prototype** above the call site (`size_t my_strlen(char *str);`) and define the body later, anywhere.
- **This is what header files (`.h`) actually are/do**: they contain function *prototypes* (signature only, no body) — not the real compiled code. `stdio.h` gives the compiler `printf`'s signature so it can type-check the call; the actual compiled `printf` machine code lives in the C standard library and gets resolved later, at the **linking** stage. Headers exist so multiple `.c` files (or call sites before a definition) can all share the same declaration via `#include`.
- Use `size_t` (not `int`) as the return type for anything representing a length/count/size — matches the real standard library convention (`strlen`, `sizeof`) and avoids semantically-wrong negative values plus signed-overflow risk on very large sizes. Print it with `%zu`, not `%d`.

## Structs

- C has no classes — `struct` groups related fields. Two declaration forms: `struct Point { int x; int y; };` (must always write `struct Point p;` — `Point` alone is just a "tag," not a type name) vs `typedef struct { int x; int y; } Point;` (creates a real type alias — `Point p;` works directly). Both forms need a trailing `;` after the closing `}` — a genuinely common, nasty-to-debug omission, since the compiler folds whatever comes next into the same broken declaration.
- **Self-referential structs (e.g. linked-list nodes)** need the hybrid form — tag name *and* typedef: `typedef struct Node { int data; struct Node *next; } Node;`. Inside the body, the typedef alias `Node` doesn't exist yet (not valid until the whole statement, including the `;`, finishes) — but the **tag** `struct Node` is visible immediately after `struct Node {`, specifically so a self-referencing pointer is possible. Only a *pointer* to the not-yet-complete type is allowed inside the body (pointer size never depends on the pointed-to type's completeness) — an actual embedded instance (`struct Node next;`, no `*`) would require infinite recursive size and is disallowed.
- A struct *definition* is just a type/blueprint — no memory exists yet, so the "always init pointers" rule doesn't apply to the member declaration itself. It applies once you create a real instance: an uninitialized `next` member holds garbage like any other pointer, until you explicitly set it (e.g. via an initializer list `{ .data = 5, .next = NULL }`).
- Access a struct member through a pointer with `->` (`current->data`), not `.` — `current->data` is shorthand for `(*current).data`. The parens in the explicit form are required because `.` binds tighter than `*`.
- Linked list convention: the last node's `next` is `NULL`, used as the traversal sentinel (`while (current != NULL) { ...; current = current->next; }`).
- Assigning a struct *value* where a pointer is expected (`Node *p = n1;` instead of `&n1`) is a type mismatch, same category as `int *p = 10;` — a pointer needs an address, not a value. Reserve `NULL` comparisons for actual pointers; comparing a plain `int` to `NULL` is misleading even if it technically compiles, since `NULL` is fundamentally `0` reinterpreted as a pointer type.

## Dynamic memory (`malloc`/`free`)

- C has no destructors, no GC, no scope-based automatic cleanup at all. `malloc` (`<stdlib.h>`) requests memory from the **heap** (separate from the stack); `free` is the only way it's ever released, and nothing calls it for you — ever.
- Always check `malloc`'s return value for `NULL` (allocation failure) before using it.
- **Heap vs. stack:** stack allocation/deallocation is automatic, fast, fixed-size at compile time, and scope-bound (reclaimed the instant the function returns). Heap allocation is manual, slower (real allocator bookkeeping), can be sized at runtime, and persists until explicitly freed — which is also the main legitimate reason to use it: data that must outlive the function that created it cannot live on the stack.
- **`free(p)` does NOT set `p` to `NULL`.** `free` is an ordinary function receiving a copy of the address (pass-by-value, same as everywhere else) — it cannot reach back and modify the caller's `p`. After `free(p)`, `p` is a **dangling pointer**: same old address, memory no longer owned. Dereferencing it afterward is undefined behavior — use-after-free — and is a major real-world security vulnerability class precisely because it often doesn't visibly fail. Observed live: reading `*p` after `free(p)` printed garbage, not the original value — likely because the allocator writes its own free-list bookkeeping directly into freed payload space.
- **Defensive habit:** manually set `p = NULL;` immediately after every `free(p)`, so accidental later use hits a checkable/crashing state instead of silently touching memory that may now belong to something else.
- **Double-free** (calling `free` twice on the same pointer) corrupts allocator bookkeeping. Observed live on Windows/MSYS2: process terminated with exit code `-1073740940` = NTSTATUS `0xC0000374` (`STATUS_HEAP_CORRUPTION`) — the OS heap manager detected the corruption and killed the process. Linux/glibc reports this as `double free or corruption`. **Critical embedded point:** this crash-and-report behavior is a courtesy from a desktop OS's heap manager — bare-metal microcontrollers typically have no equivalent detection, so the same bug there can silently corrupt memory instead of failing loudly, surfacing as an unrelated failure much later.
- **Production practice spectrum:** safety-certified domains (automotive/ISO 26262, aerospace/DO-178C) often ban or heavily restrict dynamic allocation after init, due to non-deterministic timing and long-uptime fragmentation risk. General embedded/IoT work uses it routinely, often via the RTOS's own allocator (FreeRTOS `heap_1`..`heap_5`, different tradeoffs — e.g. `heap_1` has no `free()` at all, by design). Common middle ground even where allowed: allocate everything needed once at startup, never free/reallocate during normal runtime.

## Memory leaks

- A leak: losing the only pointer to an allocated block (e.g. reassigning `p = malloc(...)` again before `free`-ing what `p` used to point to) without ever freeing it. The block stays marked "in use" by the allocator but is permanently unreachable — no crash, no garbage value, no warning. The program behaves perfectly.
- **On desktop/short-lived processes, leaks are often harmless in practice** — the OS unconditionally reclaims *all* of a process's memory the instant it exits, leaked or not.
- **On embedded firmware, leaks are dangerous specifically because there's no process exit ever.** `main` typically runs an infinite loop for the device's entire uptime (weeks/months between power cycles) — nothing ever reclaims a leak. A small per-iteration leak (e.g. one leaked block per second in a sensor loop) permanently and cumulatively shrinks a tiny heap (often just KB on an MCU) until some future `malloc` call fails (`NULL`), causinмg a real functional failure or, if unchecked, a NULL-dereference crash.
- **The classic real symptom this produces:** "worked fine in testing, fails after N days of continuous field uptime" — the failure is decoupled in time from the actual buggy line, since a short test run can't reveal a slow accumulation. This is why leak discipline (or avoiding runtime dynamic allocation entirely, per the earlier heap/production-practice notes) matters far more in long-running embedded systems than in typical short-lived desktop programs.

## Bit manipulation

- **Why it matters:** hardware registers are fixed-width integers where individual bits (or small groups) each control/report something unrelated to their neighbors (e.g. GPIO mode, pull-up enable, UART status flags). Needs surgical, single-bit-safe operations, not "treat it as one meaningful number."
- **`1 << n`** builds a mask with a single `1` at position `n` — reads directly against datasheets, which document registers by bit position number (e.g. "bit 7: TXE"), not hex value.
- **Four fixed, state-independent idioms** — correct regardless of the bit's current value, unlike ad-hoc XOR use:
  - Set bit `n`: `reg |= (1 << n);`
  - Clear bit `n`: `reg &= ~(1 << n);`
  - Toggle bit `n`: `reg ^= (1 << n);`
  - Read bit `n`: `(reg >> n) & 1;`
  - XOR only belongs to toggle — using it for "set" or "clear" only looks correct when the bit's prior state happens to cooperate; it silently breaks the moment that assumption changes. OR/AND are correct unconditionally because they're idempotent toward the desired outcome.
- **Multi-bit ranges:** OR together individual masks, or use the range formula `(1 << n) - 1` for `n` consecutive bits starting at position 0 (e.g. `(1 << 4) - 1 = 0x0F` for bits 0–3).
- **Setting a multi-bit *field* to a specific value** (the realistic embedded case — e.g. a 2-bit mode-select field) needs clear-then-set, not OR alone: `reg = (reg & ~mask) | (value << start_bit);` — OR alone can never turn an already-set bit back to `0`, so skipping the clear step breaks reconfiguration of a field that isn't starting from all-zero.
  - **Live bug pattern, hit three times in one sitting configuring GPIO `MODER`/`AFRL`:** writing the clear and set as two separate statements (`reg &= ~(mask << base); reg |= (value << base_wrong);`) with *mismatched* shift amounts between the two. The mask defines *where* the field lives; the value must land in that exact same spot, or it either leaves the field unset (if the wrong shift undershoots into already-cleared bits) or corrupts a neighboring field/pin entirely (if it overshoots past the field's boundary). Always double-check the shift amount is identical in both halves — better yet, compute it once into a variable/constant and reuse it in both places instead of retyping the number twice.
- **Left shift (`<<`)** always fills the vacated bit with `0`. **Right shift (`>>`)** fills differently depending on signedness: `unsigned` always fills with `0` (logical shift); `signed` negative values typically fill with the sign bit on GCC/Clang (arithmetic shift, preserves sign) — but this is technically **implementation-defined** by the C standard, not guaranteed portable. Arithmetic right shift ≈ floor division (rounds toward −∞), which differs from `/` (truncates toward zero) for negative values — e.g. `-2 >> 2 = -1`, but `-2 / 4 = 0`.

## `const` and `volatile`

- **`volatile`**: tells the compiler a variable can change for reasons outside the program's own visible code (real hardware flipping bits at a memory-mapped address). Forces every access to be a real memory read/write, every time — disables caching the value in a CPU register and disables reordering/eliminating "redundant-looking" accesses. Does **not** change what value is read or written — only controls *when/how often* the compiler touches memory.
  - Without it: a polling loop like `while ((status_reg & (1 << 7)) == 0) { }` can be legally optimized to read `status_reg` once, cache it, and loop forever on the stale cached value — since nothing in the visible C code writes to it, the compiler assumes it can't change. Real, common embedded bug.
  - Real register access is typically a pointer: `volatile uint8_t *status_reg = (volatile uint8_t *)0x40001000;` (address from the MCU reference manual).
- **`const`**: unrelated to hardware/caching — a purely compile-time promise that *this code* will not write through this reference. Enforced by the compiler at compile time (write attempt = compile error). Says nothing about whether the underlying memory can change through other means (hardware, another pointer).
- **Combined (`const volatile`)**, a common real idiom for read-only hardware status/flag registers: `volatile` = hardware can change it, never cache; `const` = this code must never attempt to write to it, catch that mistake at compile time. Two orthogonal guarantees — one about compiler caching behavior, one about compile-time write-protection.

## `static`

C overloads one keyword for two unrelated mechanisms, decided purely by *where* it's written — unlike Java's single "belongs to the class" meaning.

- **Inside a function, on a local variable:** changes storage duration from stack (fresh each call, destroyed on return) to a single fixed location for the program's whole lifetime. Initialized exactly once (first execution reaches that line, never again on later calls); value is retained between calls. Name stays scoped to the function only. Use case: call counters, "already initialized" one-time flags.
- **At file scope, on a global variable or function:** restricts linkage — the symbol becomes visible only within its own `.c` file, invisible to the linker from any other file (default is external linkage — visible everywhere with a matching declaration). Used to hide a module's internal implementation and avoid cross-file naming collisions. Loosely analogous to Java's `private`, but operating at the file/translation-unit level, not the class level.

## Function pointers

- A function pointer holds the address of a function's compiled code, same concept as a data pointer, different target. A function's name decays to its address in most contexts, just like an array name decays to `&array[0]` — `my_function` and `&my_function` are equivalent.
- **Declaration syntax:** `return_type (*ptr_name)(param_types);` — e.g. `void (*handler_ptr)(void);`. The parens around `*ptr_name` are mandatory; without them (`void *ptr_name(void);`) it means a function *returning* `void *`, not a pointer to a function — same precedence trap as `.` vs `->`.
- **Calling through it needs no explicit dereference**, unlike data pointers: `handler_ptr()` and `(*handler_ptr)()` are exactly equivalent and compile identically. This is a special C rule: dereferencing a function pointer yields a "function designator" that immediately auto-converts back into a function pointer in nearly every context (except `&`/`sizeof`) — so any number of extra `*`s before the call still compiles to the same thing.
- **Real embedded use:** ARM Cortex-M interrupt vector tables are literally an array of function pointers at a fixed address, read by the CPU at boot/interrupt time — entry N is the address of the handler for interrupt N. Same mechanism underlies callback registration and FreeRTOS task creation (handing the scheduler a function pointer to run).
- Array-of-function-pointers declaration: same shape as a single function pointer, with `[N]` added after the name — `void (*table[3])(void) = { fn_a, fn_b, fn_c };`.

## `enum` and `union`

- **`enum`**: just named `int` constants under the hood — no type safety (an enum variable can silently hold any `int`, unlike Java's enforced enum types). Values start at `0`, increment by `1` by default unless assigned explicitly. Use: state machine states, error/status codes, mode selectors — replaces magic numbers with names.
- **`union`**: all members share the *same* memory address; total size = size of the *largest* member (not the sum, unlike `struct`). Only one member is meaningfully "active" at a time; the compiler doesn't track which.
  - Real embedded use — **type punning**: reinterpret the same raw bytes as different types with no shifting/casting, e.g. `union { uint32_t raw; uint8_t bytes[4]; }` to decode a 32-bit value into its individual bytes (or vice versa) for UART/SPI/I2C protocol handling.
  - Caveats: which byte lands in `bytes[0]` depends on CPU **endianness** (upcoming topic). Strictly, reading a union member other than the last one written is unspecified/implementation-defined per ISO C — GCC documents and supports it as a reliable extension, and it's extremely widely used in real embedded code despite the technicality.

---
*C-language core topics complete. Next section: computer architecture & memory layout (endianness, stack/heap/data/bss/text segments, word size, alignment).*
