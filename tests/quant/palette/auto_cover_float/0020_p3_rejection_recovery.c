/* SPDX-License-Identifier: MIT */
/* P3 recovers after replacing unsupported HLS output with AUTO. */
int test_auto_cover_admission(int);

int
test_auto_cover_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(10);
}
