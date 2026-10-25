// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Humberto VALERA
/*
 * write_buf.c - pack entries into a user-space buffer, write() whole blocks.
 *
 * Same N entries as write_unbuf, but we only cross into the kernel when
 * the buffer is full: about N * ENTRY_SIZE / bufsize system calls instead
 * of N. With the default 64 KiB buffer that is one write() every 1024
 * entries. It is what stdio does for you behind every printf(), minus
 * the locking and the format parsing.
 *
 * Usage: write_buf [-b bufsize] [-r reps] [N]
 */

#include <string.h>

#include "bench.h"

/*
 * Copy entries into the buffer and flush it when full.
 *
 * Buffering is not free. Every byte is now copied twice: once here by
 * memcpy(), and once more by the kernel into the page cache. That
 * memcpy() is user time, and it is timed on purpose, because leaving it
 * out would be cheating. Fewer system calls is a trade, not a free
 * lunch, and the trade can go bad: -b 32 makes every entry cost two
 * write() calls, and a huge buffer stops fitting in the CPU cache long
 * after the system call overhead has already vanished into the noise.
 *
 * Nothing forces bufsize to be a multiple of ENTRY_SIZE, so an entry may
 * straddle two buffers. That is all the inner loop is for.
 *
 * The flushes go through write_all(), not a bare write(). On a regular
 * file the kernel nearly always takes the whole block in one go, so it
 * is still one write() per flush (strace -c will tell you). But "nearly
 * always" is not "always", and an ignored short write here drops up to
 * a whole buffer of data, not one entry.
 */
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

	/* The tail never filled a whole buffer. Don't forget it. */
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
