/* SPDX-License-Identifier: MIT */
/* Native HLS parallel or. */
int hls_native_contract(int);

int
test_hls_native_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(9);
}
