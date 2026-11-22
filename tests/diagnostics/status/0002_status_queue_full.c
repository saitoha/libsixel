/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

int
test_status_0002_status_queue_full(int argc, char **argv)
{
    char const *message;

    (void)argc;
    (void)argv;
    message = sixel_helper_format_error(SIXEL_QUEUE_FULL);
    return message != NULL &&
        strcmp(message, "runtime error: queue has no free slot") == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
