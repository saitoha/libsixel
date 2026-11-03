/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>
int test_gpu_rectangle(int policy, int method, int observation);

int
test_fhedt_gpu_rectangle(int argc, char **argv)
{
    if (argc != 3) return 1;
    return test_gpu_rectangle(SIXEL_LUT_POLICY_FHEDT,
        atoi(argv[1]) ? SIXEL_DIFFUSE_BLUENOISE_DITHER : SIXEL_DIFFUSE_NONE,
        atoi(argv[2]));
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
