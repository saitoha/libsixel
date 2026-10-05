/* SPDX-License-Identifier: MIT */
/* PNG snapshot byte frame control. */
int hls_png_contract(int);

int
test_png_snapshot_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_png_contract(6);
}
