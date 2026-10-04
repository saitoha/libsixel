/* SPDX-License-Identifier: MIT */
/* Native HLS p3 mask. */
int hls_native_contract(int);

int
test_hls_native_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(7);
}
