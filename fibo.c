#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static unsigned long long fib(unsigned int n)
{
    if (n < 2)
    {
        return n;
    }

    return fib(n - 1) + fib(n - 2);
}

// Default values:
// n = 42 runs an intentionally CPU-intensive Fibonacci workload.
// memory_mb = 0 means no extra memory is allocated unless provided as an argument.

int main(int argc, char *argv[])
{
    unsigned int n = 42;
    size_t memory_mb = 0;
    void *buffer = NULL;
    unsigned long long round = 0;

    if (argc > 1)
    {
        n = (unsigned int) atoi(argv[1]);
    }

    if (argc > 2)
    {
        memory_mb = (size_t) atoi(argv[2]);
    }

    printf("PID: %d\n", getpid());
    printf("Running CPU-intensive workload: fib(%u)\n", n);
    fflush(stdout);

    if (memory_mb > 0)
    {
        size_t bytes = memory_mb * 1024 * 1024;

        // Allocate and touch memory so the kernel really commits it.
        buffer = malloc(bytes);

        if (buffer == NULL)
        {
            perror("malloc");
            return 1;
        }

        memset(buffer, 1, bytes);

        printf("Allocated memory: %zu MB\n", memory_mb);
        fflush(stdout);
    }

    while (1)
    {
        unsigned long long result = fib(n);

        round++;

        printf("round=%llu result=%llu\n", round, result);
        fflush(stdout);
    }

    free(buffer);

    return 0;
}
