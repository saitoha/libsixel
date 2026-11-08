/* SPDX-License-Identifier: MIT */
int test_partial_refresh(int mode, int gpu);

int
test_refresh_0025(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_partial_refresh(4, 1);
}
