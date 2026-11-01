/* SPDX-License-Identifier: MIT */
int test_partial_refresh(int mode);

int
test_refresh_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_partial_refresh(13);
}
