/* SPDX-License-Identifier: MIT */
/* Verify floor runtime cap. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0070(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(1);
}
