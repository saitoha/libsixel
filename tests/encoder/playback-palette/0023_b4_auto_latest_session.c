/* SPDX-License-Identifier: MIT */
/* Verify b4 auto latest session. */
#include <sixel.h>
int test_playback_palette_case(int, int, int);

int
test_encoder_playback_palette_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_playback_palette_case(2, 1, 4);
}
