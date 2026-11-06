/* SPDX-License-Identifier: MIT */
#include <stddef.h>
int phased_playback_compare(char const *, char const *, unsigned int);

int
test_phased_playback_small_error(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_playback_compare("phased:gain=small-error", "delta:T0", 2);
}
