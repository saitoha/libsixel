/* SPDX-License-Identifier: MIT */
int test_delta_refresh(int mode);

int
test_refresh_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_refresh(6);
}
