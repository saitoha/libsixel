/* SPDX-License-Identifier: MIT */
/* Native HLS hue and gamut. */
int hls_native_contract(int);

int
test_hls_native_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(4);
}
