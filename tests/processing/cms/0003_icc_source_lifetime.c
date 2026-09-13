/* SPDX-License-Identifier: MIT */
/* ICC bytes and caller definitions may expire before transform use. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/vectors.h"

int test_cdef_0003(int argc, char **argv)
{
    unsigned char bytes[sizeof(color_icc)];
    sixel_color_def_t *definition;
    sixel_color_def_t *clone;
    double xyz[3];
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    memcpy(bytes, color_icc, sizeof(bytes));
    definition = sixel_cdef_icc(bytes, sizeof(bytes));
    memset(bytes, 0, sizeof(bytes));
    clone = sixel_cdef_clone(definition);
    sixel_cdef_free(definition);
    ok = clone != NULL && sixel_cdef_to_xyz(clone, color_vectors[0], xyz, 1);
    if (ok) {
        for (channel = 0; channel < 3; ++channel) {
            if (fabs(xyz[channel] - color_vectors[0][6 + channel]) > 2e-4) {
                ok = 0;
            }
        }
    }
    sixel_cdef_free(clone);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
