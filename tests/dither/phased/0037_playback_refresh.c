/* SPDX-License-Identifier: MIT */
#include <stddef.h>
int phased_playback_compare(char const *, char const *, unsigned int);

int
test_phased_playback_refresh(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return phased_playback_compare("phased", "delta:T48:refresh=5", 2);
}
