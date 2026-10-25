# cgroups-demo

This example puts a CPU-intensive process inside a cgroup v2 and shows
how the kernel applies the CPU and memory limits. The process is
[`fibo.c`](fibo.c): a recursive Fibonacci that runs forever. With the
second argument (in MB) it also allocates and touches some memory, to
test `memory.max`.

You need `gcc`, a kernel with **cgroup v2**, and **root** for the steps
that write in `/sys/fs/cgroup`. Run all the steps from this directory.

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

With `cpu.max` = `50000 100000` the process should use about 50% of one
core (you can see it in the system monitor), and `throttled_usec` in
`cpu.stat` keeps growing every time the kernel stops the process.

### Step 8 (optional): Test the memory limit

In step 5 fibo allocates its 32 MB before it is moved into the cgroup,
and in cgroup v2 that memory stays charged to the old cgroup (the memory
is not moved with the process). So to test `memory.max`, stop the first
fibo and start a new one directly inside the cgroup, asking for more
than the 80 MB limit:

```
kill $PID
sudo sh -c "echo \$\$ > $CG/cgroup.procs && exec ./fibo 42 100"
```

The process can't use 100 MB with a limit of 80 MB, so if you don't
have swap the OOM killer kills it (with swap, part of the memory goes to
swap instead). Check the `max`, `oom` and `oom_kill` counters:

```
cat $CG/memory.events
```
