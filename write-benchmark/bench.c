// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Humberto VALERA
/*
 * bench.c - timing and reporting harness for the write() benchmarks.
 *
 * Only the write phase is timed. Data generation, open() and close() are
 * kept out of the measurement: both programs pay for them exactly the
 * same, so all they would add is noise. The one thing allowed to differ
 * between the two programs is how they call write().
 *
 * fsync() is never called, and that is deliberate. write() to a regular
 * file does not touch the disk. It copies the data into the page cache,
 * marks the pages dirty and returns; writeback happens later, on the
 * kernel's schedule, not ours. So what we measure is the price of going
 * in and out of the kernel plus that one copy, which is the whole point.
 * Throw in an fsync() and you are benchmarking your SSD instead.
 *
 * The flip side is that the page cache, the rest of the system and the
 * CPU frequency governor all leak into the numbers. That is why every
 * test is repeated and averaged. Don't trust a single run, and don't
 * trust a busy machine.
 */

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

/* Summed over all repetitions, divided by their count when printed. */
struct result
{
	double real_ms;		/* elapsed wall-clock time */
	double user_ms;		/* CPU time in user mode */
	double sys_ms;		/* CPU time in kernel mode */
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

/**
 * write_all - write() until the whole buffer is out, or die trying
 * @fd:  destination
 * @p:   data to write
 * @len: number of bytes
 *
 * write() is allowed to write less than you asked for. It returns how
 * many bytes it actually took, and coming back for the rest is *your*
 * job, not the kernel's. Code that ignores this works for years on
 * regular files, then silently loses data the first time it meets a
 * pipe, a socket or a full disk.
 *
 * So: loop on short writes, restart on EINTR, die on anything else.
 * Every trip around the loop is one more write() system call, and
 * strace -c counts every one of them. Compare with the bare write() in
 * write_unbuf.c, which has no loop and therefore no second chances.
 */
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

/*
 * Build all N entries up front, in one contiguous block. Generating data
 * is not what we are measuring, and doing it once here means both
 * programs write byte-for-byte the same file: "entry 0000000042 ", one
 * letter repeated up to ENTRY_SIZE - 1, and a '\n'.
 */
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

		/* This also overwrites the '\0' snprintf() left. Intended. */
		memset(e + len, 'a' + i % 26, ENTRY_SIZE - len - 1);
		e[ENTRY_SIZE - 1] = '\n';
	}
	return data;
}

/*
 * One timed run of the strategy. The timed region is test->run() and
 * nothing else.
 *
 * Wall time comes from CLOCK_MONOTONIC, because the realtime clock can be
 * stepped by NTP or by root, and a benchmark where time runs backwards
 * is not a benchmark. The user/sys split, which is what this whole
 * experiment is about, comes from getrusage().
 *
 * Don't read too much into that split for short runs, though. On most
 * kernels it is sampled at the timer tick, so a run of a few ms is
 * divided up by a handful of samples. Use a bigger N.
 */
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

/*
 * Read the file back and compare it with what we meant to write. Being
 * fast is easy if you write garbage, so prove we didn't.
 */
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

	/* n == 0 means we got all the way to EOF without a mismatch. */
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

/* No N on the command line: ask for it. Garbage in means 0, i.e. usage. */
static unsigned long ask_entries(void)
{
	unsigned long n;

	printf("Number of entries to write: ");
	fflush(stdout);
	return scanf("%lu", &n) == 1 ? n : 0;
}

/**
 * bench_main - parse options, run a write strategy and print its timings
 * @argc: from main()
 * @argv: from main()
 * @test: strategy under test
 *
 * Prints the mean real, user and sys time of the write phase over all
 * repetitions, plus the resulting throughput.
 *
 * Return: EXIT_SUCCESS, or EXIT_FAILURE if the output file is wrong.
 */
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

	/* Setup is not timed. */
	data = gen_data(n);
	if (test->needs_buf)
	{
		buf = malloc(bufsize);
		if (!buf)
			die("malloc");
	}

	/*
	 * Every run gets a brand new file: unlink() it right away instead
	 * of letting the next open(O_TRUNC) reuse it. Don't "simplify" that
	 * away. ext4 treats open(O_TRUNC) + write() + close() as "replace
	 * this file" and starts writeback at close() (auto_da_alloc, see
	 * ext4(5)), so the previous run's disk I/O would bleed into the
	 * next one. An unlinked file's dirty pages are simply thrown away
	 * and usually never reach the disk at all.
	 */
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

	/*
	 * real should be close to user + sys, since all of this is CPU work.
	 * If real is way bigger, something made us wait (dirty page
	 * throttling, another task on our CPU) and the run is suspect.
	 */
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
