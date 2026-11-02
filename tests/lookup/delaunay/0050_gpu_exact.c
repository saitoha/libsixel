/* SPDX-License-Identifier: MIT */
#include <sixel.h>
int gpu_lookup_exact_test(int argc, char **argv, int lookup);

int
test_delaunay_gpu_exact(int argc, char **argv)
{
    return gpu_lookup_exact_test(argc, argv, SIXEL_LUT_POLICY_DELAUNAY);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
