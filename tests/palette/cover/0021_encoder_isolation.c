/* SPDX-License-Identifier: MIT */
/* Interleaved encoders retain independent byte and float admission. */
int test_auto_cover_admission(int);

int
test_auto_cover_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(11);
}
