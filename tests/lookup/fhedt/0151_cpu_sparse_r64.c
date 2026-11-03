/* SPDX-License-Identifier: MIT */
/* Verify cpu r64 sparse and colliding seed labels. */
int test_fh_sparse(int gpu, int resolution, int refine,
    int weighted, int dense);

int
test_fh_cpu_sparse_r64(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_sparse(0, 64, 0, 0, 0);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
