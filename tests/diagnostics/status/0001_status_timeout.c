/*
 * SPDX-License-Identifier: MIT
 *
 * Keep the timeout status distinct from successful interruption.  Async
 * decoder jobs need a result that says "the job is still running" without
 * making callers treat the wait as a completed decode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include <sixel.h>

int
test_status_0001_status_timeout(int argc, char **argv)
{
    char const *message;
    int failed;

    (void)argc;
    (void)argv;

    failed = 0;
    if (SIXEL_TIMEOUT != (SIXEL_RUNTIME_ERROR | 0x0008)) {
        fprintf(stderr, "SIXEL_TIMEOUT has an unexpected value\n");
        failed = 1;
    }
    if (!SIXEL_FAILED(SIXEL_TIMEOUT)) {
        fprintf(stderr, "SIXEL_TIMEOUT must be a failed status\n");
        failed = 1;
    }
    message = sixel_helper_format_error(SIXEL_TIMEOUT);
    if (message == NULL ||
            strcmp(message, "runtime error: operation timed out") != 0) {
        fprintf(stderr, "SIXEL_TIMEOUT has an unexpected message\n");
        failed = 1;
    }

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
