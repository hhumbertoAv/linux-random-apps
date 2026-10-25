# linux-random-apps

Small programs that I use to show how some parts of the Linux kernel
work: eBPF, cgroups, real-time scheduling, W^X memory protection and the
cost of a system call. They are examples to read and run, not real
tools. Each example has its own directory and its own README with the
steps.

## Examples

| Directory | What it does | You need |
|---|---|---|
| [`ebpf-ctrlz-count/`](ebpf-ctrlz-count/) | An eBPF program that counts **SIGTSTP** (Ctrl+Z) with the `signal:signal_generate` tracepoint. It is loaded and pinned with `bpftool`. | clang, bpftool |
| [`cgroups-demo/`](cgroups-demo/) | Limits a CPU-intensive process (`fibo.c`) with cgroup v2 (`cpu.max` and `memory.max`), and shows how to check it with `cpu.stat` / `memory.current`. | gcc |
| [`rt-scheduling/`](rt-scheduling/) | One `SCHED_FIFO` real-time thread per core, and what `sched_rt_runtime_us` is for. | gcc, pthreads |
| [`mmap-wxorx/`](mmap-wxorx/) | Why W^X is important: a buffer overflow changes a function pointer so it points to a writable **and** executable `mmap` region. This is a bad practice on purpose. | gcc |
| [`write-benchmark/`](write-benchmark/) | Measures the cost of a system call: the same data written with one `write()` per entry vs. with a user-space buffer. | gcc, make |

## Building

Go into the directory of each example and compile it:

```sh
# cgroups-demo
gcc -O0 -Wall -Wextra -o fibo fibo.c

# rt-scheduling
gcc -O2 -Wall -Wextra -o rt_application rt_application.c -lpthread

# mmap-wxorx
gcc -O0 -Wall -Wextra -o mmap_wxorx mmap_wxorx.c

# write-benchmark (it has a Makefile)
make

# ebpf-ctrlz-count (first generate vmlinux.h, see its README)
clang -O2 -g -target bpf -c ctrlz_count.bpf.c -o ctrlz_count.bpf.o
```

Some examples need **root** (the cgroups and eBPF ones) and a recent
kernel with cgroup v2 and BTF. Read the README of each example before
running anything as root.

## License

Apache-2.0, © 2026 Humberto VALERA (see [`LICENSE`](LICENSE)).

Only `ebpf-ctrlz-count/` is GPL-2.0. An eBPF program that uses GPL-only
kernel helpers (like the ones used by `BPF_CORE_READ`) must declare a
GPL-compatible license to the kernel, so that file can't be Apache-2.0.
