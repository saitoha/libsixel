/* SPDX-License-Identifier: MIT */
/* P3 resolves omitted capacity before mask admission. */
int test_auto_cover_admission(int);

int
test_auto_cover_0024(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(14);
}
