/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0012(int argc, char **argv)
{
    static char const *const specs[] = {
        "", "p3", "auto:srgb", "icc:", "icc:-", "icc:relative:",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06:gamma=2.2,2.2",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06:gamma=2.2,2.2,2.2,2.2",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06:gamma=2.2,0,2.2",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06:gamma=2.2,nan,2.2",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,0:srgb",
        "rgb:.3127,.329,.64,.33,.3,.6,.15,.06: srgb"
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
        path = "unchanged";
        intent = 42;
        ok = sixel_cdef_parse(specs[i], &definition, &path, &intent);
        sixel_cdef_free(definition);
        if (ok || strcmp(path, "unchanged") != 0 || intent != 42) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
