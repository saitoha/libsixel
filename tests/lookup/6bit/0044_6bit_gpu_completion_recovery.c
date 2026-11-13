/* SPDX-License-Identifier: MIT */
/* Verify 6bit GPU completion recovery. */
int test_rgb_cell_gpu(int bits, int observation);

int
test_bit6_gpu_completion_recovery(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_rgb_cell_gpu(6, 10);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
