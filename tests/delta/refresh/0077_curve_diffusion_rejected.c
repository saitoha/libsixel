/* SPDX-License-Identifier: MIT */
/* Verify curve diffusion rejected. */
int test_refresh_adaptive_boundary(int);

int
test_delta_refresh_0077(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_refresh_adaptive_boundary(8);
}
