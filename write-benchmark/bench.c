// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Humberto VALERA

// bench.c: timing and report code for the write() benchmarks.
//
// Only the write phase is timed. The data generation, open() and close()
// are not measured, because they are the same for both programs and they
// would only add noise. The only difference between the two programs is
// how they call write().
//
// We never call fsync(), on purpose. A write() to a regular file doesn't
// go to the disk: the kernel copies the data into the page cache, marks
// the pages as dirty and returns. The data goes to the disk later
// (writeback), when the kernel decides. So here we measure the cost of
// entering and leaving the kernel plus that copy, and that is what we
// want. With an fsync() we would be measuring the disk instead.
//
// The problem is that the page cache, the other processes and the CPU
// frequency also change the numbers. That's why each test is repeated and
// we print the mean. Don't trust a single run, or a run on a busy machine.

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "bench.h"

// Times added up over all the repetitions (divided by reps when printed).
struct result
{
	double real_ms;		// wall-clock time
	double user_ms;		// CPU time in user mode
	double sys_ms;		// CPU time in kernel mode
};

void die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static double ts_ms(const struct timespec *t)
{
	return t->tv_sec * 1e3 + t->tv_nsec / 1e6;
}

static double tv_ms(const struct timeval *t)
{
	return t->tv_sec * 1e3 + t->tv_usec / 1e3;
}

// write_all(): call write() until the whole buffer is written, or die.
//
// write() can write fewer bytes than we asked for (a "short write"). It
// returns the number of bytes it really wrote, and the program has to call
// it again for the rest, the kernel doesn't do it for us. Code that
// ignores this usually works fine with regular files, but it can lose data
// with a pipe, a socket or a full disk.
//
// So: if the write is short we loop, if we get EINTR we try again, and any
// other error is fatal. Each loop is one more write() system call (you can
// count them with strace -c). Compare it with write_unbuf.c, which calls
// write() only once per entry and has no loop.
void write_all(int fd, const char *p, size_t len)
{
	while (len > 0)
	{
		ssize_t n = write(fd, p, len);

		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
			die("write");
		p += n;
		len -= n;
	}
}

// Generate all the N entries at the beginning, in one block of memory.
// Generating the data is not what we want to measure, and this way both
// programs write exactly the same file. Each entry is "entry 0000000042 ",
// then the same letter repeated until ENTRY_SIZE - 1, and a '\n'.
static char *gen_data(unsigned long n)
{
	char *data = malloc(n * ENTRY_SIZE);
	unsigned long i;

	if (!data)
		die("malloc");

	for (i = 0; i < n; i++)
	{
		char *e = data + i * ENTRY_SIZE;
		int len = snprintf(e, ENTRY_SIZE, "entry %010lu ", i);

		// This also overwrites the '\0' written by snprintf(), on purpose.
		memset(e + len, 'a' + i % 26, ENTRY_SIZE - len - 1);
		e[ENTRY_SIZE - 1] = '\n';
	}
	return data;
}

// One timed run of the test. Only test->run() is inside the timed part.
//
// The wall-clock time comes from CLOCK_MONOTONIC, because the realtime
// clock can jump (NTP or root can change it). The user/sys split comes
// from getrusage(), and that split is the important part of this example.
//
// But be careful with short runs: on most kernels the user/sys time is
// sampled at each timer tick, so a run of a few ms only gets a few
// samples. Use a bigger N.
static void bench_once(const struct bench_test *test, const char *data,
		       unsigned long n, char *buf, size_t bufsize,
		       struct result *r)
{
	struct timespec t0, t1;
	struct rusage u0, u1;
	int fd;

	fd = open(test->path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		die("open");

	getrusage(RUSAGE_SELF, &u0);
	clock_gettime(CLOCK_MONOTONIC, &t0);

	test->run(fd, data, n, buf, bufsize);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	getrusage(RUSAGE_SELF, &u1);

	if (close(fd) < 0)
		die("close");

	r->real_ms += ts_ms(&t1) - ts_ms(&t0);
	r->user_ms += tv_ms(&u1.ru_utime) - tv_ms(&u0.ru_utime);
	r->sys_ms += tv_ms(&u1.ru_stime) - tv_ms(&u0.ru_stime);
}

// Read the file again and compare it with the data we wanted to write.
// This checks that the program really wrote the correct data, not only
// that it was fast.
static int verify(const char *path, const char *data, size_t len)
{
	char chunk[DEF_BUFSIZE];
	size_t off = 0;
	ssize_t n;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		die("open");

	while ((n = read(fd, chunk, sizeof(chunk))) > 0)
	{
		if (off + n > len || memcmp(chunk, data + off, n))
			break;
		off += n;
	}
	if (n < 0)
		die("read");
	close(fd);

	// n == 0 means we reached EOF and all the data matched.
	return n == 0 && off == len;
}

static double mbps(double bytes, double ms)
{
	return ms > 0 ? bytes / 1e6 / (ms / 1e3) : 0.0;
}

static void usage(const char *prog, const struct bench_test *test)
{
	fprintf(stderr, "Usage: %s%s [-r reps] [N]\n"
		"  N         number of entries (prompted for if omitted)\n",
		prog, test->needs_buf ? " [-b bytes]" : "");
	if (test->needs_buf)
		fprintf(stderr, "  -b bytes  buffer size (default %d)\n",
			DEF_BUFSIZE);
	fprintf(stderr, "  -r reps   repetitions (default %d)\n", DEF_REPS);
	exit(EXIT_FAILURE);
}

// If N is not in the command line, ask for it. If the input is not a
// number we return 0, and then bench_main() prints the usage.
static unsigned long ask_entries(void)
{
	unsigned long n;

	printf("Number of entries to write: ");
	fflush(stdout);
	return scanf("%lu", &n) == 1 ? n : 0;
}

// bench_main(): parse the options, run the write test and print the times.
//
// It prints the mean real, user and sys time of the write phase over all
// the repetitions, and the throughput.
//
// Returns EXIT_SUCCESS, or EXIT_FAILURE if the output file is not correct.
int bench_main(int argc, char **argv, const struct bench_test *test)
{
	const char *optstr = test->needs_buf ? "b:r:h" : "r:h";
	struct result r = { 0 };
	size_t bufsize = DEF_BUFSIZE;
	char *data, *buf = NULL;
	int reps = DEF_REPS;
	unsigned long n;
	size_t len;
	int opt, i;

	while ((opt = getopt(argc, argv, optstr)) != -1)
	{
		switch (opt)
		{
		case 'b':
			bufsize = strtoul(optarg, NULL, 10);
			break;
		case 'r':
			reps = atoi(optarg);
			break;
		default:
			usage(argv[0], test);
		}
	}

	n = optind < argc ? strtoul(argv[optind], NULL, 10) : ask_entries();
	if (n == 0 || bufsize == 0 || reps < 1)
		usage(argv[0], test);
	len = n * ENTRY_SIZE;

	// The setup is not timed.
	data = gen_data(n);
	if (test->needs_buf)
	{
		buf = malloc(bufsize);
		if (!buf)
			die("malloc");
	}

	// Each run uses a new file: we call unlink() just after each run,
	// instead of letting the next open(O_TRUNC) reuse the same file.
	// This is important, don't remove it. ext4 treats open(O_TRUNC) +
	// write() + close() as "replace this file" and starts the writeback
	// at close() (auto_da_alloc, see ext4(5)), so the disk I/O of one run
	// would affect the next one. When the file is unlinked, its dirty
	// pages are just dropped and usually never go to the disk.
	for (i = 0; i < reps; i++)
	{
		bench_once(test, data, n, buf, bufsize, &r);

		if (i == 0 && !verify(test->path, data, len))
		{
			fprintf(stderr, "%s: output file does not match\n",
				test->path);
			unlink(test->path);
			return EXIT_FAILURE;
		}
		unlink(test->path);
	}

	// real should be close to user + sys, because all this is CPU work.
	// If real is much bigger, the program had to wait for something
	// (dirty page throttling, another process on the same CPU) and the
	// result is not reliable.
	printf("%s: %lu entries, mean of %d runs\n", test->name, n, reps);
	if (test->needs_buf)
		printf("  buffer      %10zu bytes\n", bufsize);
	printf("  real        %10.3f ms\n", r.real_ms / reps);
	printf("  user        %10.3f ms\n", r.user_ms / reps);
	printf("  sys         %10.3f ms\n", r.sys_ms / reps);
	printf("  throughput  %10.2f MB/s\n", mbps(len, r.real_ms / reps));

	free(buf);
	free(data);
	return EXIT_SUCCESS;
}
