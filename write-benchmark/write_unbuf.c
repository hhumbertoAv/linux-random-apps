// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Humberto VALERA
/*
 * write_unbuf.c - one write() system call per entry.
 *
 * Writing N entries costs N round trips into the kernel and back. Each
 * one moves a measly 64 bytes, so nearly all the time goes into the trip
 * itself: the mode switch (which the CPU vulnerability mitigations made
 * a lot more expensive than it used to be) plus the fixed bookkeeping
 * write() does around those 64 bytes, like looking up the file, locking
 * the inode and finding the right page in the page cache. All of that
 * is kernel work, and it shows up as sys time.
 *
 * Usage: write_unbuf [-r reps] [N]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bench.h"

/*
 * The bare system call, no wrapper. One entry, one write(), and the loop
 * below *is* the system call count: nothing hides behind it.
 *
 * The price of calling write() directly is checking its return value
 * ourselves, and it has two ways to let you down. It can return -1 with
 * errno set, or it can return a short count, fewer bytes than we asked
 * for. write_all() in bench.c handles the short count by looping. We
 * don't: for 64 bytes going into a regular file, a short write in
 * practice means the filesystem (or RLIMIT_FSIZE) just ran out, and
 * retrying would only fetch the real error one call later. So anything
 * but a full write is fatal. No EINTR either: we install no signal
 * handlers, so there is nothing to interrupt us.
 *
 * What you must never do is call write() and ignore what it returns.
 * That is not "fast", it is a data-loss bug waiting for a full disk.
 */
static void run_unbuffered(int fd, const char *data, unsigned long n,
			   char *buf, size_t bufsize)
{
	unsigned long i;

	(void)buf;		/* no buffer: that is the whole point */
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
