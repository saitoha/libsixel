/* SPDX-License-Identifier: MIT */
#include <stddef.h>
int delta_compare_pixel_test(int, int, int, int);
int delta_compare_option_test(char const *, char const *, char const *,
    int, int, int);

int
test_delta_compare_0065(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return delta_compare_option_test("delta:Cq",
        "full:Cq", "", 1, 1, 0);
}
