/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <sixel.h>
int test_gpu_direct_parity(int method, int ncolors);

/* A forced direct apply must be visible through the process statistics:
 * the backend reports itself and the dispatch and busy counters advance. */
int
test_gpu_direct_0008_statistics(int argc, char **argv)
{
    sixel_gpu_statistics_t before;
    sixel_gpu_statistics_t after;

    (void)argc;
    (void)argv;
    if (sixel_gpu_get_statistics(NULL) != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "NULL statistics output was accepted\n");
        return 1;
    }
    if (SIXEL_FAILED(sixel_gpu_get_statistics(&before))) {
        fprintf(stderr, "initial statistics query failed\n");
        return 1;
    }
    if (test_gpu_direct_parity(SIXEL_DIFFUSE_NONE, 64) != 0) {
        return 1;
    }
    if (SIXEL_FAILED(sixel_gpu_get_statistics(&after))) {
        fprintf(stderr, "statistics query after dispatch failed\n");
        return 1;
    }
    if (!after.available || after.backend[0] == '\0' ||
            after.device[0] == '\0') {
        fprintf(stderr, "statistics do not name the backend/device\n");
        return 1;
    }
    if (after.dispatches <= before.dispatches ||
            after.busy_nanoseconds <= before.busy_nanoseconds) {
        fprintf(stderr, "statistics did not advance: dispatches %llu->%llu "
            "busy %llu->%llu\n", before.dispatches, after.dispatches,
            before.busy_nanoseconds, after.busy_nanoseconds);
        return 1;
    }
    return 0;
}
