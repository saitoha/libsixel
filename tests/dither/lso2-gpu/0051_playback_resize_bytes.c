/* SPDX-License-Identifier: MIT */
int lso2_gpu_playback_resize_compare(int);

int
test_lso2_gpu_playback_resize_bytes(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_playback_resize_compare(0);
}
