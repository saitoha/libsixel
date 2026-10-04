/* SPDX-License-Identifier: MIT */
/* Byte encoding resolves omitted capacity before reserving its mask slot. */
int test_auto_cover_admission(int);

int
test_auto_cover_0025(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(15);
}
