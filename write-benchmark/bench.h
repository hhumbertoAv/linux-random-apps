/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 Humberto VALERA */
/*
 * bench.h - shared harness for the write() system call benchmarks.
 *
 * Each benchmark program only provides the write strategy under test;
 * option parsing, data generation, timing and reporting live in bench.c.
 * Keep it that way. Both strategies must be measured by the very same
 * code, or the comparison is worthless.
 */
#ifndef _BENCH_H
#define _BENCH_H

#include <stddef.h>

#define ENTRY_SIZE	64		/* bytes per entry, including '\n' */
#define DEF_BUFSIZE	(64 * 1024)	/* 64 KiB */
#define DEF_REPS	5

/**
 * struct bench_test - description of one write strategy
 * @name:      short name printed in the report
 * @path:      scratch file the strategy writes to
 * @needs_buf: non-zero if @run uses the user-space buffer (enables -b)
 * @run:       write @n entries of ENTRY_SIZE bytes from @data to @fd
 *
 * Only @run is timed. @buf and @bufsize are valid only if @needs_buf is set.
 */
struct bench_test
{
	const char *name;
	const char *path;
	int needs_buf;
	void (*run)(int fd, const char *data, unsigned long n,
		    char *buf, size_t bufsize);
};

void die(const char *msg);
void write_all(int fd, const char *p, size_t len);
int bench_main(int argc, char **argv, const struct bench_test *test);

#endif /* _BENCH_H */
