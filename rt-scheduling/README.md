# rt-scheduling

One real-time thread per online CPU, each running under `SCHED_FIFO` at
priority 50, and a one-line invitation to disarm the safety net that
keeps them from freezing your machine.

[`rt_application.c`](rt_application.c) asks the kernel how many CPUs are
online (`sysconf(_SC_NPROCESSORS_ONLN)`), spawns that many threads, and
has each one promote itself to `SCHED_FIFO` with `pthread_setschedparam()`.
A `SCHED_FIFO` thread runs until it blocks or yields; nothing of equal or
lower priority gets the CPU while it is runnable. Each worker then loops:
compute `fib(42)` (a second or two of pure CPU), print the result, and
`sleep(5)`. The `sleep()` is the only reason the machine stays usable —
it is what lets everything else run in the gaps.

## Build

```
gcc -O2 -Wall -Wextra -o rt_application rt_application.c -lpthread
```

## Run

Getting a real-time scheduling policy needs privilege (`CAP_SYS_NICE`, or
a non-zero `RLIMIT_RTPRIO`). Run it as root:

```
sudo ./rt_application
```

Stop it with **Ctrl+C**.

Without privilege `pthread_setschedparam()` fails with `EPERM`. The
threads notice (it returns an error number directly, not via `errno`),
print a warning, and keep running under the ordinary `SCHED_OTHER`
policy — so you still get output, just without the real-time behaviour.

## The footgun

The comment at the top of the file says:

```
Try setting the /proc/sys/kernel/sched_rt_runtime_us file to -1 for fun :)
```

By default the kernel refuses to let real-time tasks monopolise the CPU:
out of every `sched_rt_period_us` (1,000,000 µs) it reserves at most
`sched_rt_runtime_us` (950,000 µs, i.e. 95%) for `SCHED_FIFO`/`SCHED_RR`,
leaving a 5% sliver for everything else. That sliver is your way back in.

Set `sched_rt_runtime_us` to `-1` and the throttle is gone. These
particular threads `sleep(5)` between bursts, so they yield the CPU on
their own and the box survives. Remove the sleep (or raise the priority
and spin) with the throttle disabled and a runaway `SCHED_FIFO` thread
can take a core and never give it back — including from the very shell
you would use to kill it. Try it in a VM you can hard-reset, not on
anything you care about.
