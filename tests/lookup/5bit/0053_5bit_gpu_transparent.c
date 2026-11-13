/* SPDX-License-Identifier: MIT */
/* Verify 5bit GPU transparent. */
int test_rgb_cell_gpu(int bits, int observation);

int
test_bit5_gpu_transparent(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_rgb_cell_gpu(5, 19);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
