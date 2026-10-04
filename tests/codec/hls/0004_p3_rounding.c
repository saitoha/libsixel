/* SPDX-License-Identifier: MIT */
/* Native HLS p3 rounding. */
int hls_native_contract(int);

int
test_hls_native_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(3);
}
