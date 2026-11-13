/* SPDX-License-Identifier: MIT */
/* Verify same modifiers keep age. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0076(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(7);
}
