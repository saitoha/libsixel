/* SPDX-License-Identifier: MIT */
/* P3 defaults admit byte input despite a byte preference. */
int test_auto_cover_admission(int);

int
test_auto_cover_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(9);
}
