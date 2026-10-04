/* SPDX-License-Identifier: MIT */
/* Explicit corners still grow between two AUTO calls. */
int test_auto_cover_explicit(void);

int
test_auto_cover_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_explicit();
}
