/* SPDX-License-Identifier: MIT */
/* Verify or float wire levels. */
int or_native_writer_case(int scenario);

int
test_or_native_0040(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return or_native_writer_case(0);
}
