/* SPDX-License-Identifier: MIT */
/* Verify 6bit GPU shared override. */
int test_bitlut_encode(int bits, int observation);

int
test_bit6_gpu_shared_override(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_bitlut_encode(6, 2);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
