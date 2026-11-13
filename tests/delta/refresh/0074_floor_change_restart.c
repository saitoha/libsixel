/* SPDX-License-Identifier: MIT */
/* Verify floor change restart. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0074(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(5);
}
