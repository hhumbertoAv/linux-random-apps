/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 Humberto VALERA */

// bench.h: common code for the write() benchmarks.
//
// Each program (write_unbuf.c and write_buf.c) only implements its own way
// of writing the data. Everything else (options, data generation, timing
// and the report) is in bench.c, so both programs are measured with the
// same code and the comparison is fair.

#ifndef _BENCH_H
#define _BENCH_H

#include <stddef.h>

#define ENTRY_SIZE	64		// bytes per entry, including the '\n'
#define DEF_BUFSIZE	(64 * 1024)	// default buffer size: 64 KiB
#define DEF_REPS	5		// default number of repetitions

// Description of one write strategy:
//   name:      name printed in the report
//   path:      temporary file where the data is written
//   needs_buf: 1 if run() uses the user-space buffer (enables the -b option)
//   run:       writes n entries of ENTRY_SIZE bytes from data to fd
//
// Only run() is timed. buf and bufsize are only valid if needs_buf is set.
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
