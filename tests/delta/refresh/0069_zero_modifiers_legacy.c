/* SPDX-License-Identifier: MIT */
/* Verify zero modifiers legacy. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0069(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(0);
}
