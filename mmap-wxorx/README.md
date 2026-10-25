# mmap-wxorx

A small, deliberate demonstration of why **W^X** ("write xor execute")
exists: one page of memory that is writable *and* executable at the same
time, plus a memory-safety bug, is all you need to turn attacker-supplied
bytes into running code.

> This is a teaching anti-pattern. The injected bytes here are harmless
> (`mov eax, 42; ret` — they just return 42), the overflow is hand-sized
> to stay inside one struct, and nothing leaves the process. It is here
> to be understood, not reused.

## What [`mmap_wxorx.c`](mmap_wxorx.c) does

1. `mmap()`s a single page with `PROT_READ | PROT_WRITE | PROT_EXEC` —
   the forbidden combination. On a W^X system a page is meant to be
   writable *or* executable, never both at once.
2. Copies six bytes of x86-64 machine code into that page: `b8 2a 00 00
   00` (`mov eax, 0x2a`) followed by `c3` (`ret`). On any other
   architecture these bytes are meaningless.
3. Builds a `struct Frame { char buffer[10]; void (*callback)(void); }`
   and points `callback` at a normal, safe function.
4. Overflows `buffer` with a payload sized to reach exactly the end of
   `callback`, overwriting the function pointer with the address of the
   `mmap` page.
5. Calls `callback`. Control jumps into the writable page and runs the
   injected bytes, so the program prints `Result from injected code: 42`.

The point: step 1 (a W^X-violating page) and step 4 (a buffer overflow)
are each survivable on their own. Together they are arbitrary code
execution. `NX`/`DEP` and W^X policies exist precisely to make step 1
impossible, so that step 4 crashes instead of executing.

## Build

Keep the optimiser off so the compiler does not see through the overflow
and "optimise" it away:

```
gcc -O0 -Wall -Wextra -o mmap_wxorx mmap_wxorx.c
```

If your toolchain's stack protector interferes, add `-fno-stack-protector`.

## Run

```
./mmap_wxorx
```

Expected output:

```
Before overflow:
Normal function executed.
After overflow:
Result from injected code: 42
```

This is **x86-64 only** — the injected bytes are raw x86-64 instructions.
On a hardened system that forbids `PROT_WRITE | PROT_EXEC` mappings (some
SELinux policies, `MDWE`/`memory-deny-write-execute`), the `mmap()` or
the jump into the page will fail, which is exactly the protection the
demo is about.
