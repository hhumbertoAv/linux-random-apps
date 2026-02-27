# linux-random-apps

## 1. `ctrlz_count.bpf.c`

This example counts how many times the kernel generates **SIGTSTP** (typically triggered by **Ctrl+Z**) using the tracepoint `signal:signal_generate`.

### Step 1: Generate `vmlinux.h`

We include `vmlinux.h` because it brings in the kernel’s real type/struct definitions generated from BTF, so the eBPF program can compile and correctly interpret the `ctx` variable in the program. It also enables CO-RE (`BPF_CORE_READ`) to read fields portably across kernel versions.

```
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

### Step 2: Compile the eBPF program

Compile the BPF C source into an object file (`.o`) targeting the BPF backend.

```
clang -O2 -g -target bpf -c ctrlz_count.bpf.c -o ctrlz_count.bpf.o
```

### Step 3: Prepare a pin directory in bpffs

Create a fresh directory under `/sys/fs/bpf/` to store the pinned eBPF objects (the attached program/link and its maps), so they stay loaded and you can access them later by their filesystem paths.
When the program runs, it updates the map: it looks up the counter at key 0 and increments it by 1. (It’s not a normal on-disk file, but an in-memory handle to an object living in the kernel.)

```
sudo rm -rf /sys/fs/bpf/ctrlz
sudo mkdir -p /sys/fs/bpf/ctrlz
```

### Step 4: Load, auto-attach to the tracepoint, and pin the maps

Load the program into the kernel, let bpftool auto-attach it to the tracepoint declared in the ELF section (SEC("tracepoint/...")), and then pin the resulting link plus all the program’s maps under /sys/fs/bpf/ctrlz so they stay persistent and can be inspected later by path (e.g., dumping the counter map).

```
sudo bpftool prog load ctrlz_count.bpf.o /sys/fs/bpf/ctrlz/ctrlz_link autoattach pinmaps /sys/fs/bpf/ctrlz
```

### Step 5: Read the counter

Dumps the current value stored in the pinned counter map (key 0). Since the eBPF program increments it each time it sees SIGTSTP, this number is the total SIGTSTP events counted since the map was created or last cleared.

```
sudo bpftool map dump pinned /sys/fs/bpf/ctrlz/counter
```
