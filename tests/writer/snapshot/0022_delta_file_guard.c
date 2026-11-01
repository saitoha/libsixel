/* SPDX-License-Identifier: MIT */
/* PNG snapshot delta contract. */
int test_png_delta_file_guard(int);

int
test_png_snapshot_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_png_delta_file_guard(0);
}
