/* SPDX-License-Identifier: MIT */
/* Preserve M0 parser acceptance separately from normal ICC color accuracy. */
#include <stdlib.h>
#include "src/cms.h"
#include "tests/data/color-numeric/minimal-profiles.h"

int
test_cms_0024(int argc, char **argv)
{
    static double const input[3] = {.2, .4, .6};
    unsigned char const *profiles[3];
    size_t lengths[3];
    sixel_color_def_t *definition;
    double xyz[3];
    int i;
    int ok;

    (void)argc;
    (void)argv;
    profiles[0] = color_minimal_display_p3;
    profiles[1] = color_minimal_forward_lut;
    profiles[2] = color_minimal_paired_lut;
    lengths[0] = sizeof(color_minimal_display_p3);
    lengths[1] = sizeof(color_minimal_forward_lut);
    lengths[2] = sizeof(color_minimal_paired_lut);
    ok = 1;
    for (i = 0; ok && i < 3; ++i) {
        definition = sixel_cdef_icc(profiles[i], lengths[i]);
        /* Missing descriptive tags and the old forward-only display class
         * do not invalidate this intentionally permissive builtin parser.
         * Successful parsing does not establish full ICC conformance. */
        ok = definition != NULL &&
            sixel_cdef_to_xyz(definition, input, xyz, 0) &&
            sixel_cdef_output_ready(definition, 0, 0) == (i != 1);
        sixel_cdef_free(definition);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
