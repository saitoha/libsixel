/* SPDX-License-Identifier: MIT */
/* Explicit AUTO overrides an active environment cover budget. */
int test_auto_cover_admission(int);

int
test_auto_cover_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(8);
}
