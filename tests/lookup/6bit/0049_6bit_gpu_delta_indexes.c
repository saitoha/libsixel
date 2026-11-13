/* SPDX-License-Identifier: MIT */
/* Verify 6bit GPU delta indexes. */
int test_rgb_cell_gpu(int bits, int observation);

int
test_bit6_gpu_delta_indexes(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_rgb_cell_gpu(6, 15);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
