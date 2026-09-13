/* SPDX-License-Identifier: MIT */
/* Reject incomplete/extra profile bytes without adopting their ownership. */
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"

int test_cdef_0009(int argc, char **argv)
{
    unsigned char bytes[sizeof(color_icc) + 1u];
    sixel_color_def_t *definition;
    size_t length;

    (void)argc;
    (void)argv;
    memcpy(bytes, color_icc, sizeof(color_icc));
    bytes[sizeof(color_icc)] = 0;
    for (length = sizeof(color_icc) - 1u; length <= sizeof(bytes);
         length += 2u) {
        definition = sixel_cdef_icc(bytes, length);
        if (definition != NULL) {
            sixel_cdef_free(definition);
            return EXIT_FAILURE;
        }
    }
    /* Valid length with invalid profile signature reaches parser unwind. */
    bytes[36] = 0;
    definition = sixel_cdef_icc(bytes, sizeof(color_icc));
    if (definition != NULL) {
        sixel_cdef_free(definition);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
