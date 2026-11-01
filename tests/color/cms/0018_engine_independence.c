/* SPDX-License-Identifier: MIT */
/* A legacy engine selector cannot change an owned builtin definition. */
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"

int test_cdef_0012(int argc, char **argv)
{
    static double const input[3] = {.3, .4, .5};
    sixel_color_def_t *definition;
    sixel_cms_engine_t saved;
    double expected[3];
    double actual[3];
    int engine;
    int ok;

    (void)argc;
    (void)argv;
    saved = sixel_cms_get_engine();
    definition = sixel_cdef_icc(color_icc, sizeof(color_icc));
    ok = sixel_cdef_to_xyz(definition, input, expected, 0);
    for (engine = SIXEL_CMS_ENGINE_NONE;
         ok && engine <= SIXEL_CMS_ENGINE_COLORSYNC; ++engine) {
        sixel_cms_set_engine((sixel_cms_engine_t)engine);
        ok = sixel_cdef_to_xyz(definition, input, actual, 0) &&
            memcmp(expected, actual, sizeof(expected)) == 0;
    }
    sixel_cms_set_engine(saved);
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
