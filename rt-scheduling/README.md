# rt-scheduling

This example creates one real-time thread per online CPU. Each thread
uses the `SCHED_FIFO` policy with priority 50.

[`rt_application.c`](rt_application.c) gets the number of online CPUs
with `sysconf(_SC_NPROCESSORS_ONLN)` and creates one thread per CPU.
Each thread changes its own policy to `SCHED_FIFO` with
`pthread_setschedparam()`, and then runs a loop: it computes `fib(42)`
(one or two seconds of CPU), prints the result and calls `sleep(5)`.

(remember: a `SCHED_FIFO` thread keeps the CPU until it blocks or
yields, so threads with the same or lower priority can't run on that CPU
in the meantime. Here the `sleep(5)` is what gives the CPU back to the
rest of the system.)

### Step 1: Compile

```
gcc -O2 -Wall -Wextra -o rt_application rt_application.c -lpthread
```

### Step 2: Run it as root

To use a real-time policy you need privileges (`CAP_SYS_NICE`, or a
non-zero `RLIMIT_RTPRIO`), so run it with sudo:

```
sudo ./rt_application
```

Stop it with **Ctrl+C**.

If you run it without sudo, `pthread_setschedparam()` fails with
`EPERM`. The threads print a warning and keep running with the normal
`SCHED_OTHER` policy, so you still see the output, but without real-time
scheduling.

### Step 3 (for fun): Disable the RT throttling

The comment at the top of `rt_application.c` says:

```
Try setting the /proc/sys/kernel/sched_rt_runtime_us file to -1 for fun :)
```

By default the kernel doesn't let real-time tasks use 100% of the CPU.
In every `sched_rt_period_us` (1000000 us = 1 s), real-time tasks can
use at most `sched_rt_runtime_us` (950000 us = 95%). The other 5% is for
the normal tasks, like your shell.

```
cat /proc/sys/kernel/sched_rt_period_us   #expected 1000000
cat /proc/sys/kernel/sched_rt_runtime_us  #expected 950000
```

With `-1` this limit is removed:

```
echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us
```

With this program nothing bad happens, because the threads call
`sleep(5)` and give the CPU back. But if you remove the `sleep()`, there
is one thread per core that never stops, so they can take all the cores
and the system freezes (even the shell that you would use to kill them).
Do this only in a VM that you can reset!!!

To go back to the default value:

```
echo 950000 | sudo tee /proc/sys/kernel/sched_rt_runtime_us
```
