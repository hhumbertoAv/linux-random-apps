# linux-random-apps

A grab-bag of small, self-contained programs, each one poking at a
different corner of the Linux kernel: eBPF tracing, cgroup resource
limits, real-time scheduling, W^X memory protection and the cost of a
system call. They exist to be read and run, not shipped — each in its
own directory, with its own README.

## Examples

| Directory | What it shows | Toolchain |
|---|---|---|
| [`ebpf-ctrlz-count/`](ebpf-ctrlz-count/) | An eBPF program that counts **SIGTSTP** (Ctrl+Z) using the `signal:signal_generate` tracepoint, loaded and pinned with `bpftool`. | clang, bpftool |
| [`cgroups-demo/`](cgroups-demo/) | Capping a CPU-bound process (`fibo.c`) with cgroup v2 `cpu.max` and `memory.max`, then watching it from `cpu.stat` / `memory.current`. | gcc |
| [`rt-scheduling/`](rt-scheduling/) | `SCHED_FIFO` real-time threads, one per core, and what the `sched_rt_runtime_us` throttle is there to protect you from. | gcc, pthreads |
| [`mmap-wxorx/`](mmap-wxorx/) | Why W^X exists: a buffer overflow redirects a function pointer into a writable **and** executable `mmap` region. A deliberate anti-pattern. | gcc |
| [`write-benchmark/`](write-benchmark/) | The price of a system call, measured: the same data written one `write()` per entry vs. packed into a user-space buffer. | gcc, make |

## Building

Each example builds on its own. In short:

```sh
# C examples
cd cgroups-demo   && gcc -O0 -Wall -Wextra -o fibo fibo.c
cd rt-scheduling  && gcc -O2 -Wall -Wextra -o rt_application rt_application.c -lpthread
cd mmap-wxorx     && gcc -O0 -Wall -Wextra -o mmap_wxorx mmap_wxorx.c

# benchmark (has a Makefile)
cd write-benchmark && make

# eBPF (needs clang + a vmlinux.h from your running kernel)
cd ebpf-ctrlz-count && clang -O2 -g -target bpf -c ctrlz_count.bpf.c -o ctrlz_count.bpf.o
```

Several examples poke at kernel state and need **root** (the cgroup and
eBPF ones in particular) and a reasonably recent kernel with cgroup v2
and BTF. Read the per-example README before running anything as root.

## License

Apache-2.0, © 2026 Humberto VALERA (see [`LICENSE`](LICENSE)).

`ebpf-ctrlz-count/` stays GPL-2.0: an eBPF program that uses GPL-only
kernel helpers (here `bpf_probe_read`, via `BPF_CORE_READ`) must declare
a GPL-compatible license to the kernel, so Apache-2.0 would be
inconsistent for that one file.
