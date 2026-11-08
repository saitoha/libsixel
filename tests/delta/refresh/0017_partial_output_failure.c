/* SPDX-License-Identifier: MIT */
int test_partial_refresh(int mode, int gpu);

int
test_refresh_0017(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_partial_refresh(9, 0);
}
