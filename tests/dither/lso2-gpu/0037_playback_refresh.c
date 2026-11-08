/* SPDX-License-Identifier: MIT */
#include <stddef.h>
int lso2_gpu_playback_compare(char const *, char const *, unsigned int);

int
test_lso2_gpu_playback_refresh(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_playback_compare("lso2", "delta:T48:refresh=5", 2);
}
