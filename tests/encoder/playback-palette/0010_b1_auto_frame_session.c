/* SPDX-License-Identifier: MIT */
/* Verify b1 auto frame session. */
#include <sixel.h>
int test_playback_palette_case(int, int, int);

int
test_encoder_playback_palette_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_playback_palette_case(2, 0, 1);
}
