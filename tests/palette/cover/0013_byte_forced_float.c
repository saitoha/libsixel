/* SPDX-License-Identifier: MIT */
/* Explicit float precision admits a byte source with AUTO. */
int test_auto_cover_admission(int);

int
test_auto_cover_0013(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(3);
}
