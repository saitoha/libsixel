/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <sixel.h>
int gpu_failure_test(int lookup, int fault);

int
test_delaunay_gpu_failure(int argc, char **argv)
{
    if (argc != 2 || atoi(argv[1]) < 1 || atoi(argv[1]) > 3) {
        return EXIT_FAILURE;
    }
    return gpu_failure_test(SIXEL_LUT_POLICY_DELAUNAY, atoi(argv[1]));
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
