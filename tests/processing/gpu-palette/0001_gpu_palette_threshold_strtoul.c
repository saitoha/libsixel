/*
 * SPDX-License-Identifier: MIT
 *
 * Pin the historical strtoul() contract before the threshold moves into the
 * shared option registry.  The policy is intentionally tested without a GPU.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/compat_stub.h"
#include "src/gpu-palette.h"

typedef struct gpu_threshold_case {
    char const *text;
    size_t expected;
} gpu_threshold_case_t;

int
test_gpupal_0001_strtoul(int argc, char **argv)
{
    gpu_threshold_case_t const cases[] = {
        { "", (size_t)262144u },
        { "0", (size_t)0u },
        { "17", (size_t)17u },
        { " 17", (size_t)17u },
        { "+17", (size_t)17u },
        { "-1", (size_t)ULONG_MAX },
        { "17x", (size_t)262144u },
        {
            "999999999999999999999999999999999999999999999999999999",
            (size_t)ULONG_MAX
        }
    };
    size_t index;
    size_t actual;

    (void)argc;
    (void)argv;
    index = 0u;
    actual = 0u;

    while (index < sizeof(cases) / sizeof(cases[0])) {
        if (sixel_compat_setenv("SIXEL_GPU_PALETTE_THRESHOLD",
                                cases[index].text) != 0) {
            fprintf(stderr, "failed to set palette threshold case %zu\n",
                    index + 1u);
            return EXIT_FAILURE;
        }
        actual = sixel_gpu_palette_auto_threshold();
        if (actual != cases[index].expected) {
            fprintf(stderr,
                    "palette threshold case %zu: got %zu, expected %zu\n",
                    index + 1u,
                    actual,
                    cases[index].expected);
            return EXIT_FAILURE;
        }
        ++index;
    }

    (void)sixel_compat_setenv("SIXEL_GPU_PALETTE_THRESHOLD", "");
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
