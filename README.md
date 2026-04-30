# linux-random-apps

## 1. `ctrlz_count.bpf.c`

This example counts how many times the kernel generates **SIGTSTP** (typically triggered by **Ctrl+Z**) using the tracepoint `signal:signal_generate`.

### Step 1: Generate `vmlinux.h`

We include `vmlinux.h` so the eBPF program knows the kernel’s real struct/type definitions (exported via BTF) and can compile with the correct `ctx` layout (remember: `ctx` is the tracepoint event context: a pointer to the data payload for that event, e.g., it contains fields like the signal number). With that BTF type info available, CO-RE helpers like `BPF_CORE_READ()` can also adapt field accesses across kernel versions, even if struct offsets change.

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

Dumps the current value stored in the pinned counter map. Since the eBPF program increments it each time it sees SIGTSTP, this number is the total SIGTSTP events counted since the map was created.

```
sudo bpftool map dump pinned /sys/fs/bpf/ctrlz/counter
```

## 2. `cgroup example`


### Step 1: Verify cgroups v2 filesystem

```
stat -fc %T /sys/fs/cgroup #expected cgroup2fs

```

### Step 2: Create a new demo cgroup:

```
CG=/sys/fs/cgroup/os-demo
sudo mkdir -p $CG
```

### Step 3: Enable CPU and memory controllers in the parent cgroup

```
# Ask the current cgroup: "which resource controllers are available here?"
cat /sys/fs/cgroup/cgroup.controllers

# If not already done, tell the parent cgroup: "enable CPU and memory control for my children"
echo "+cpu +memory" | sudo tee /sys/fs/cgroup/cgroup.subtree_control
```

### Step 4: Set CPU and memory limits for the new cgroup

```
# Limit the cgroup to 50% of one CPU core
echo "50000 100000" | sudo tee $CG/cpu.max

# Limit memory usage to 80 MB
echo "80M" | sudo tee $CG/memory.max
```

### Step 5: Compile, run and get the PID of the fibo.c application

```
gcc -O0 -Wall -Wextra -o fibo fibo.c
./fibo 42 32 &

PID=$!
```

### Step 6: Move the running process into the cgroup

```
echo $PID | sudo tee $CG/cgroup.procs
```

### Step 7: Verify

```
cat /proc/$PID/cgroup #Expected: 0::/os-demo
cat $CG/cgroup.procs
watch -n 1 "cat $CG/cpu.stat; echo; cat $CG/memory.current; echo; cat $CG/memory.events"
# Open your system monitor
```

