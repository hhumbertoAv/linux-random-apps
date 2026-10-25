// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Humberto VALERA

// write_unbuf.c: one write() system call per entry.
//
// Writing N entries means N trips into the kernel and back. Each one only
// writes 64 bytes, so almost all the time is spent in the trip itself: the
// mode switch (more expensive now because of the mitigations for CPU
// vulnerabilities) plus the work that write() always does around those 64
// bytes, like finding the file, locking the inode and finding the right
// page in the page cache. All that is kernel work, so it shows up as sys
// time.
//
// Usage: write_unbuf [-r reps] [N]

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bench.h"

// Here we call write() directly, without any wrapper. One entry, one
// write(), so the number of loops is the number of system calls.
//
// Because we call write() directly, we have to check its return value
// ourselves. It can fail in two ways: it can return -1 (and set errno), or
// it can return a short count (fewer bytes than we asked for). write_all()
// in bench.c handles the short count with a loop, but here we don't: for
// 64 bytes going to a regular file, a short write normally means that the
// filesystem (or RLIMIT_FSIZE) is full, and if we try again we just get
// the real error in the next call. So anything that is not a full write is
// a fatal error. We don't check EINTR either, because we don't install any
// signal handler, so nothing can interrupt us.
//
// WARN: never call write() and ignore its return value. It looks faster,
// but you can lose data when the disk is full.
static void run_unbuffered(int fd, const char *data, unsigned long n,
			   char *buf, size_t bufsize)
{
	unsigned long i;

	(void)buf;		// not used: this version has no buffer
	(void)bufsize;

	for (i = 0; i < n; i++)
	{
		ssize_t ret = write(fd, data + i * ENTRY_SIZE, ENTRY_SIZE);

		if (ret < 0)
			die("write");
		if (ret != ENTRY_SIZE)
		{
			fprintf(stderr, "write: short write, %zd of %d bytes\n",
				ret, ENTRY_SIZE);
			exit(EXIT_FAILURE);
		}
	}
}

static const struct bench_test test =
{
	.name	= "unbuffered",
	.path	= "bench_unbuf.tmp",
	.run	= run_unbuffered,
};

int main(int argc, char **argv)
{
	return bench_main(argc, argv, &test);
}
