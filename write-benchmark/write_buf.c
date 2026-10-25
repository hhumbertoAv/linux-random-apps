// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Humberto VALERA

// write_buf.c: pack the entries into a user-space buffer and write() the
// whole buffer at once.
//
// Same N entries as write_unbuf, but we only enter the kernel when the
// buffer is full: about N * ENTRY_SIZE / bufsize system calls instead of N.
// With the default 64 KiB buffer that is one write() every 1024 entries.
// This is more or less what stdio does for you with printf(), but without
// the locking and the format parsing.
//
// Usage: write_buf [-b bufsize] [-r reps] [N]

#include <string.h>

#include "bench.h"

// Copy the entries into the buffer and write it when it is full.
//
// Buffering also has a cost: every byte is copied two times, once here
// with memcpy() and once more by the kernel into the page cache. That
// memcpy() is user time, and we time it on purpose, because it is part of
// the cost of buffering. A buffer can also make things worse: with -b 32
// every entry needs two write() calls, and a very big buffer doesn't fit
// in the CPU cache anymore (and at that point the system call cost is
// already very small).
//
// bufsize doesn't have to be a multiple of ENTRY_SIZE, so one entry can be
// split between two buffers. That's why there is an inner loop.
//
// The buffer is written with write_all(), not with a simple write(). On a
// regular file the kernel almost always takes the whole block in one call,
// so it is still one write() per buffer (check it with strace -c). But it
// is not guaranteed, and if we ignore a short write here we can lose up to
// a whole buffer of data, not only one entry.
static void run_buffered(int fd, const char *data, unsigned long n,
			 char *buf, size_t bufsize)
{
	size_t used = 0;
	unsigned long i;

	for (i = 0; i < n; i++)
	{
		const char *src = data + i * ENTRY_SIZE;
		size_t left = ENTRY_SIZE;

		while (left > 0)
		{
			size_t chunk = bufsize - used;

			if (chunk > left)
				chunk = left;
			memcpy(buf + used, src, chunk);
			used += chunk;
			src += chunk;
			left -= chunk;

			if (used == bufsize)
			{
				write_all(fd, buf, used);
				used = 0;
			}
		}
	}

	// Write the last part of the data (it didn't fill a whole buffer).
	if (used > 0)
		write_all(fd, buf, used);
}

static const struct bench_test test =
{
	.name		= "buffered",
	.path		= "bench_buf.tmp",
	.needs_buf	= 1,
	.run		= run_buffered,
};

int main(int argc, char **argv)
{
	return bench_main(argc, argv, &test);
}
