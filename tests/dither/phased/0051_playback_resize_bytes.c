/* SPDX-License-Identifier: MIT */
int phased_playback_resize_compare(int);

int
test_phased_playback_resize_bytes(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_playback_resize_compare(0);
}
