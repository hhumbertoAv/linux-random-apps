# write-benchmark

This example measures the cost of the `write()` system call. Two small
programs write the same N entries (64 bytes each) to a file, in two
different ways:

- `write_unbuf`: one `write()` per entry. N entries = N system calls.
- `write_buf`: the entries are copied into a user-space buffer (64 KiB
  by default) and the whole buffer is written at once, with
  `write_all()`. More or less N * 64 / bufsize system calls.

Both programs use the same code to measure (`bench.c`), only the write
phase is timed, and at the end the file is read again and checked, so we
are sure that both programs really wrote the same data.

You only need `gcc` and `make`.

### Step 1: Compile

```
make
```

`make clean` removes the binaries, the objects and the `*.tmp` files.

### Step 2: Run

```
./write_unbuf [-r reps] [N]
./write_buf [-b bytes] [-r reps] [N]
```

- `N`: number of entries to write (if you don't give it, the program
  asks for it)
- `-b bytes`: buffer size, only for write_buf (default 65536)
- `-r reps`: number of repetitions for the mean (default 5)

Or run both programs, one after the other:

```
make run N=1000000
```

Each program prints the mean of all the repetitions:

- `real`: wall-clock time of the write phase
- `user`: CPU time in user mode
- `sys`: CPU time in kernel mode
- `throughput`: MB/s, calculated from `real`

Example with 1000000 entries (on your machine the numbers will be
different):

```
unbuffered: 1000000 entries, mean of 5 runs
  real           504.851 ms
  user            31.224 ms
  sys            472.877 ms
  throughput      126.77 MB/s
buffered: 1000000 entries, mean of 5 runs
  buffer           65536 bytes
  real            23.794 ms
  user             4.677 ms
  sys             19.148 ms
  throughput     2689.76 MB/s
```

Look at the unbuffered case: almost all the time is `sys`. Writing 64
bytes is very fast, what takes time is entering and leaving the kernel
one million times.

### Step 3: Count the system calls with strace

```
make strace N=10000
```

You should see N `write()` calls for write_unbuf and about
N * 64 / 65536 for write_buf, plus a few more for the text that the
programs print to stdout.

### Step 4 (optional): Try different buffer sizes

A bigger buffer is not always better:

```
./write_buf -b 32 100000         # two write() per entry: worse than no buffer!
./write_buf -b 4096 100000
./write_buf -b 67108864 100000   # after some size, a bigger buffer doesn't help
```

### Step 5 (optional): write() vs write_all()

`write()` can write fewer bytes than you asked for (a short write). You
can force one with a file size limit of 1000 bytes. The `trap` ignores
SIGXFSZ, so the process is not killed and `write()` returns an error
instead:

```
(trap '' XFSZ; prlimit --fsize=1000 ./write_unbuf -r 1 100)
(trap '' XFSZ; prlimit --fsize=1000 ./write_buf -r 1 100)
```

write_unbuf checks the simple `write()` and reports the short write.
write_buf uses `write_all()`, which calls `write()` again for the rest
of the data and then gets the real error from the kernel (`EFBIG`). You
can see it with `strace -e trace=write`. When they fail, both programs
leave their temporary file, `make clean` removes it.

## Notes

- `fsync()` is never called, on purpose. A `write()` to a regular file
  only copies the data into the page cache, the disk is not part of this
  test. But if you write a lot of data, the kernel starts to slow down
  the writes (dirty page throttling) and then the disk matters again.
- The user/sys times come from `getrusage()`, and on most kernels they
  are sampled at each timer tick. For runs of a few ms they are not
  precise, so use a bigger N.
- Other processes, the CPU frequency and the state of the page cache
  also change the numbers. Run it on a quiet machine, run it more than
  once, and if `real` is much bigger than `user + sys`, the program had
  to wait for something and the result is not reliable.

## Files

- `bench.h`: interface between the common code and the programs
- `bench.c`: options, data generation, timing, report and `write_all()`
- `write_unbuf.c`: one `write()` per entry
- `write_buf.c`: buffered version, written with `write_all()`
- `Makefile`: targets `all`, `run`, `strace` and `clean`

## License

Apache-2.0, see the SPDX headers in the source files.
