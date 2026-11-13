/* SPDX-License-Identifier: MIT */
/* Verify floor bounds atomic. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0072(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(3);
}
