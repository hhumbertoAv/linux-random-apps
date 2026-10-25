# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Humberto VALERA

CC	:= gcc
CFLAGS	:= -O2 -Wall -Wextra -std=gnu11
PROGS	:= write_unbuf write_buf
N	?= 100000

all: $(PROGS)

$(PROGS): %: %.o bench.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c bench.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Run both under the same conditions, one after the other.
run: $(PROGS)
	./write_unbuf $(N)
	./write_buf $(N)

# Count write() calls from outside; a single repetition keeps it readable.
strace: $(PROGS)
	strace -c -e trace=write ./write_unbuf -r 1 $(N)
	strace -c -e trace=write ./write_buf -r 1 $(N)

clean:
	rm -f $(PROGS) *.o *.tmp

.PHONY: all run strace clean
