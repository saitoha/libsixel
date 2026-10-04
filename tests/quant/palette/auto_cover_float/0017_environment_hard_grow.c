/* SPDX-License-Identifier: MIT */
/* Environment hard mode and growth leave native AUTO off. */
int test_auto_cover_admission(int);

int
test_auto_cover_0017(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(7);
}
