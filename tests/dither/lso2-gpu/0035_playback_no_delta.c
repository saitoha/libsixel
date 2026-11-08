/* SPDX-License-Identifier: MIT */
#include <stddef.h>
int lso2_gpu_playback_compare(char const *, char const *, unsigned int);

int
test_lso2_gpu_playback_no_delta(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lso2_gpu_playback_compare("lso2", NULL, 2);
}
