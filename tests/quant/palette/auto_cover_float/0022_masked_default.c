/* SPDX-License-Identifier: MIT */
/* Omitted cover excludes hidden RGB and reserves its mask slot. */
int test_auto_cover_admission(int);

int
test_auto_cover_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_admission(12);
}
