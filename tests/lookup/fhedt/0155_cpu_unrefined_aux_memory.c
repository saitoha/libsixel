/* SPDX-License-Identifier: MIT */
/* Verify cpu unrefined grids omit auxiliary storage. */
int test_fh_aux_memory(int resolution, int refine);

int
test_fh_cpu_unrefined_aux_memory(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_aux_memory(64, 0);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
