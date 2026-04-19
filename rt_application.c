//Try setting the /proc/sys/kernel/sched_rt_runtime_us file to -1 for fun :)  

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RT_PRIORITY 50
#define FIB_N 42

int num_cores;

unsigned long long fibonacci(int n)
{
    if (n <= 1)
        return (unsigned long long)n;

    return fibonacci(n - 1) + fibonacci(n - 2);
}

static void *worker(void *arg)
{
    struct sched_param param;
    int rc;
    long id = (long)arg;
    unsigned long long result;

    param.sched_priority = RT_PRIORITY;

    rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    printf("Thread %ld running with SCHED_FIFO priority %d\n",
           id, RT_PRIORITY);

    while (1)
    {
        result = fibonacci(FIB_N);

        printf("Thread %ld: fib(%d) = %llu\n",
               id, FIB_N, result);

        sleep(5);
    }

    return NULL;
}

int main(void)
{
    pthread_t *threads;
    int i;

    num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    threads = malloc(sizeof(pthread_t) * num_cores);

    printf("Launching %d RT threads\n", num_cores);

    for (i = 0; i < num_cores; i++)
    {
        if (pthread_create(&threads[i], NULL, worker, (void *)(long)i) != 0)
        {
            perror("pthread_create");
            free(threads);
            return 1;
        }
    }

    for (i = 0; i < num_cores; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    return 0;
}
