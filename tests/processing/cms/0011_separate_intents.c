/* SPDX-License-Identifier: MIT */
/* Encoding and measurement select independent, executable intent slots. */
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/paired-lut.h"

int test_cdef_0011(int argc, char **argv)
{
    unsigned char bytes[sizeof(color_paired_lut)];
    sixel_color_def_t *definition;
    int ok;

    (void)argc;
    (void)argv;
    memcpy(bytes, color_paired_lut, sizeof(bytes));
    /* The second tag in this fixture is B2A0. Move only its intent to 1. */
    if (memcmp(bytes + 144, "B2A0", 4) != 0) {
        return EXIT_FAILURE;
    }
    bytes[147] = '1';
    definition = sixel_cdef_icc(bytes, sizeof(bytes));
    ok = definition != NULL && sixel_cdef_output_ready(definition, 1, 0)
        && !sixel_cdef_output_ready(definition, 0, 1)
        && !sixel_cdef_output_ready(definition, 1, 1)
        && !sixel_cdef_output_ready(definition, 3, 0);
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
