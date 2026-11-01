/* SPDX-License-Identifier: MIT */
/* Native HLS parallel body. */
int hls_native_contract(int);

int
test_hls_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(8);
}
