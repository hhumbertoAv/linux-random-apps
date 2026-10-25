# mmap-wxorx

This example shows why **W^X** ("write xor execute") is important. If a
memory page is writable *and* executable at the same time, and the
program has a buffer overflow bug, an attacker can make the program run
their own bytes as code.

> Note: this is a bad practice on purpose, only to learn. The injected
> bytes are harmless (`mov eax, 42; ret`, they only return 42), the
> overflow stays inside one struct, and nothing goes outside the process.

## What [`mmap_wxorx.c`](mmap_wxorx.c) does

1. Calls `mmap()` to get one page with `PROT_READ | PROT_WRITE | PROT_EXEC`.
   This is exactly what W^X forbids: a page should be writable *or*
   executable, never both.
2. Copies 6 bytes of x86-64 machine code into that page: `b8 2a 00 00 00`
   (`mov eax, 0x2a`) and `c3` (`ret`). On other architectures these
   bytes mean nothing.
3. Creates a `struct Frame { char buffer[10]; void (*callback)(void); }`
   and sets `callback` to a normal function.
4. Overflows `buffer` with a payload that reaches the end of `callback`,
   so the function pointer now has the address of the `mmap` page.
5. Calls `callback`. The CPU jumps to the page and runs the injected
   bytes, and the program prints `Result from injected code: 42`.

So, step 1 (a page that breaks W^X) or step 4 (a buffer overflow) alone
are not enough, but both together = arbitrary code execution. `NX`/`DEP`
and the W^X policies try to make step 1 impossible, so that step 4
crashes the program instead of running the attacker's code.

### Step 1: Compile

Use `-O0`, so the compiler doesn't change or remove the overflow:

```
gcc -O0 -Wall -Wextra -o mmap_wxorx mmap_wxorx.c
```

If the stack protector of your compiler gets in the way, add
`-fno-stack-protector`.

### Step 2: Run

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

This only works on **x86-64**, because the injected bytes are x86-64
instructions. On a hardened system that doesn't allow
`PROT_WRITE | PROT_EXEC` mappings (some SELinux policies,
`MDWE`/memory-deny-write-execute), the `mmap()` or the jump to the page
fails, and that is exactly the protection that this demo is about.
