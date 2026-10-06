/* SPDX-License-Identifier: MIT */
/* PNG snapshot delta contract. */
int test_png_delta_options(int);

int
test_png_snapshot_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_png_delta_options(6);
}
