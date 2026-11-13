/* SPDX-License-Identifier: MIT */
/* Verify b4 frame frame session. */
#include <sixel.h>
int test_playback_palette_case(int, int, int);

int
test_encoder_playback_palette_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_playback_palette_case(0, 0, 4);
}
