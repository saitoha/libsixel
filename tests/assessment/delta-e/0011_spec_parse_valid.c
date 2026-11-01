/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0011(int argc, char **argv)
{
    static char const *const specs[] = {
        "auto", "srgb", "display-p3", "icc:profile.icc",
        "icc:perceptual:profile.icc", "icc:relative:profile.icc",
        "icc:saturation:profile.icc",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06:srgb",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06:gamma=2.2,2.3,2.4"
    };
    sixel_color_def_t *definition;
    char const *path;
    int intent;
    size_t i;
    int ok;
    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(specs) / sizeof(specs[0]); ++i) {
        definition = NULL;
        ok = sixel_cdef_parse(specs[i], &definition, &path, &intent);
        sixel_cdef_free(definition);
        if (!ok || (i == 4 && intent != 0) || (i == 6 && intent != 2)) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
