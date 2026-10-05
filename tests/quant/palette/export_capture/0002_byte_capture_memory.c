/* SPDX-License-Identifier: MIT */
/* Verify palette export byte capture memory. */
#include <stddef.h>
int test_export_memory(int, char const *);
int test_export_reuse(char const *, int);

int
test_export_capture_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_export_memory(0, NULL);
}
