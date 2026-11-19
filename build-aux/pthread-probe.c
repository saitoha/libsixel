/* SPDX-License-Identifier: MIT */
/* A serial build needs neither the pthread declarations nor the library. */
#include <pthread.h>
#include <stddef.h>

static void *
probe_worker(void *arg)
{
    return arg;
}

int
main(void)
{
    pthread_t worker;

    if (pthread_create(&worker, NULL, probe_worker, NULL) != 0) {
        return 1;
    }
    return pthread_join(worker, NULL) != 0;
}
