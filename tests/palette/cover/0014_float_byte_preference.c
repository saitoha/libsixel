/* SPDX-License-Identifier: MIT */
/* A byte preference does not narrow an already-float source. */
int test_auto_cover_admission(int);

int
test_auto_cover_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(4);
}
