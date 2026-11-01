/* SPDX-License-Identifier: MIT */
/* AUTO leaves an otherwise byte-eligible job on its byte path. */
int test_auto_cover_admission(int);

int
test_auto_cover_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(2);
}
