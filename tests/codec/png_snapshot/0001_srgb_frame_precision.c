/* SPDX-License-Identifier: MIT */
/* PNG snapshot srgb frame precision. */
int hls_png_contract(int);

int
test_png_snapshot_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_png_contract(0);
}
