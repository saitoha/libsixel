/* SPDX-License-Identifier: MIT */
#include <stddef.h>
int phased_playback_compare(char const *, char const *, unsigned int);

int
test_phased_playback_single_slot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_playback_compare("phased", "delta:T0", 1);
}
