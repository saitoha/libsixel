/* SPDX-License-Identifier: MIT */
/* Native HLS byte control. */
int hls_native_contract(int);

int
test_hls_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return hls_native_contract(6);
}
