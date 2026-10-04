/* SPDX-License-Identifier: MIT */
/* Plain AUTO preserves native extended palette values. */
int test_auto_cover_admission(int);

int
test_auto_cover_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(1);
}
