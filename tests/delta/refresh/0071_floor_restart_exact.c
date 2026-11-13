/* SPDX-License-Identifier: MIT */
/* Verify floor restart exact. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0071(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(2);
}
