/*
 * SPDX-License-Identifier: MIT
 *
 * Lock the library default thread counts for processing and direct decoding.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"
#include "src/decoder-parallel.h"
#include "src/threading.h"

static int
thread_default_expect(char const *name, int actual, int expected)
{
    if (actual == expected) {
        return 1;
    }

    fprintf(stderr,
            "%s resolved to %d, expected %d\n",
            name,
            actual,
            expected);
    return 0;
}

int
test_threadpool_0002_thread_default_counts(int argc, char **argv)
{
    int processing_expected;
    int processing_actual;
    int decoder_actual;

    /* Shell regressions use the same native topology probe as the library.
     * The ordinary entry below still asserts conservative library defaults. */
    if (argc == 2 && strcmp(argv[1], "converter-counts") == 0) {
        processing_expected = sixel_get_performance_threads();
        processing_expected = sixel_threads_normalize(processing_expected);
        printf("%d %d %d\n", processing_expected,
               sixel_threads_normalize(0),
               processing_expected < 2 ? processing_expected : 2);
        return EXIT_SUCCESS;
    }

#if SIXEL_ENABLE_THREADS
    processing_expected = sixel_get_performance_threads();
    if (processing_expected > 4) {
        processing_expected = 4;
    }
#else
    processing_expected = 1;
#endif

    if (sixel_compat_setenv("SIXEL_ENCODER_THREADING_POLICY", "") != 0) {
        fprintf(stderr, "failed to clear SIXEL_ENCODER_THREADING_POLICY\n");
        return EXIT_FAILURE;
    }

    if (sixel_compat_setenv("SIXEL_ENCODER_THREADING_GLOBAL_BUDGET", "") != 0) {
        return EXIT_FAILURE;
    }
    processing_actual = sixel_threads_resolve();
    decoder_actual = sixel_decoder_parallel_resolve_threads();

    if (!thread_default_expect("processing default",
                               processing_actual,
                               processing_expected)) {
        return EXIT_FAILURE;
    }
    if (!thread_default_expect("direct decoder default",
                               decoder_actual,
                               1)) {
        return EXIT_FAILURE;
    }

    printf("# library-default=%d decoder-default=%d\n",
           processing_actual, decoder_actual);
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
