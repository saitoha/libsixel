/* SPDX-License-Identifier: MIT */
/* P3 snap can be enabled and disabled on the same encoder. */
int test_auto_cover_admission(int);

int
test_auto_cover_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(10);
}
