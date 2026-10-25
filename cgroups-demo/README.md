# cgroups-demo

Put a CPU-bound process under a cgroup v2 and watch the kernel enforce
the limits. The workload is [`fibo.c`](fibo.c): a naive recursive
Fibonacci that burns CPU forever, and optionally allocates and touches
some memory so there is something for `memory.max` to bite on.

Requires `gcc`, a kernel with **cgroup v2**, and **root** for every step
that writes under `/sys/fs/cgroup`. All steps run from this directory.

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

With `cpu.max` set to `50000 100000` the process should sit at roughly
half a core, and `cpu.stat`'s `throttled_usec` should keep rising as the
kernel parks it. `./fibo 42 32` also commits 32 MB; raise that past the
80 MB `memory.max` to see `memory.events` count the reclaim.
