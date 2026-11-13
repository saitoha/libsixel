/* SPDX-License-Identifier: MIT */
/* Verify b1 latest frame session. */
#include <sixel.h>
int test_playback_palette_case(int, int, int);

int
test_encoder_playback_palette_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_playback_palette_case(1, 0, 1);
}
