/* SPDX-License-Identifier: MIT */
int gpu_failure_test(int lookup, int fault);

int
test_gpu_failure_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gpu_failure_test(13, 2);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
