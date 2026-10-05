/* SPDX-License-Identifier: MIT */
/* Verify palette export reuse count. */
#include <stddef.h>
int test_export_memory(int, char const *);
int test_export_reuse(char const *, int);

int
test_export_capture_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_export_reuse(argv[1], 0);
}
