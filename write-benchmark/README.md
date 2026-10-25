write() system call benchmark
=============================

Two tiny programs that write the same N fixed-size entries (64 bytes
each) to a file, in two different ways:

  write_unbuf   one write() per entry. N entries, N system calls.

  write_buf     entries are packed into a user-space buffer (64 KiB by
                default) and written a whole block at a time, through
                write_all(). Roughly N * 64 / bufsize system calls.

The point is to show what a user/kernel transition costs. Both programs
are measured by the same harness (bench.c), only the write phase is
timed, and the output file is read back and checked, so neither of them
can win by cheating.


BUILDING
--------

You need gcc and make. That's it.

	make

	make clean	# binaries, objects and any leftover *.tmp files


RUNNING
-------

	./write_unbuf [-r reps] [N]
	./write_buf [-b bytes] [-r reps] [N]

	N         number of entries to write (asked for if omitted)
	-b bytes  buffer size, write_buf only (default 65536)
	-r reps   repetitions to average over (default 5)

Or run both, one after the other:

	make run N=1000000

Each program prints the mean of all repetitions:

	real        wall-clock time of the write phase
	user        CPU time spent in user mode
	sys         CPU time spent in kernel mode
	throughput  MB/s, from real

Example, 1000000 entries on one machine (yours will differ):

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

Note where the time goes in the unbuffered case: almost all of it is
sys. Moving 64 bytes is nothing; getting in and out of the kernel a
million times is not.


COUNTING THE SYSTEM CALLS
-------------------------

Don't take the programs' word for it, ask strace:

	make strace N=10000

Expect N write() calls from write_unbuf and about N * 64 / 65536 from
write_buf, plus one more each for the report printed to stdout.


THINGS TO TRY
-------------

Fewer system calls is a trade, not a free lunch:

	./write_buf -b 32 100000	# two write()s per entry: worse than
					# not buffering at all
	./write_buf -b 4096 100000
	./write_buf -b 67108864 100000	# past some point a bigger buffer
					# buys you nothing

write() versus write_all(). write() may write less than you asked for
(a short write). Force one with a 1000-byte file size limit; the trap
keeps SIGXFSZ from simply killing the process:

	(trap '' XFSZ; prlimit --fsize=1000 ./write_unbuf -r 1 100)
	(trap '' XFSZ; prlimit --fsize=1000 ./write_buf -r 1 100)

write_unbuf checks the bare write() and reports the short write.
write_buf goes through write_all(), which comes back for the rest and
gets the real error (EFBIG) from the kernel. Run the second one under
strace -e trace=write to watch it happen. Both leave their scratch file
behind when they fail; make clean gets rid of it.


CAVEATS
-------

 - fsync() is never called, on purpose. write() to a regular file only
   copies into the page cache; the disk is not part of this experiment.
   Write enough data and dirty page throttling will drag it back in.

 - The user/sys split comes from getrusage(), which on most kernels is
   sampled at the timer tick. For runs of a few milliseconds it is
   mostly noise. Use a bigger N.

 - Other load, CPU frequency scaling and the state of the page cache
   all show up in the numbers. Run on a quiet machine, run more than
   once, and if real is much bigger than user + sys, something made
   the program wait and the run is suspect.


FILES
-----

	bench.h        interface between the harness and the programs
	bench.c        option parsing, data generation, timing, reporting,
	               write_all()
	write_unbuf.c  one bare write() per entry
	write_buf.c    buffered, flushed with write_all()
	Makefile       all, run, strace, clean


LICENSE
-------

GPL-2.0, see the SPDX headers in the source files.
