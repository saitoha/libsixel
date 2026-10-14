/* SPDX-License-Identifier: MIT
 * Documentation probe: read encoded Display P3 triples from stdin and
 * print continuous mapping results from the library built beside this file.
 * This is a maintainer tool, not a public API or an independent oracle.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/gamut.h"
#include "src/colorspace.h"

int
main(int argc, char **argv)
{
    sixel_allocator_t *allocator = NULL;
    sixel_color_def_t *p3 = NULL;
    sixel_color_def_t *srgb = NULL;
    sixel_color_def_t *linear = NULL;
    sixel_color_xform_t *transform = NULL;
    sixel_gamut_t *mapper[3] = {NULL, NULL, NULL};
    FILE *profile = NULL;
    unsigned char *bytes = NULL;
    long length;
    int policies[3] = {SIXEL_GAMUT_LOCAL_MINDE,
        SIXEL_GAMUT_EDGE_SEEKER, SIXEL_GAMUT_RAY_TRACE};
    double input[3];
    double values[5][3];
    double lab[3];
    int i;
    int j;
    int count;
    int result = EXIT_FAILURE;

    if (argc == 1) {
        p3 = sixel_cdef_name("display-p3");
    } else if (argc == 2) {
        /* The encoded comparison uses the exact embedded fixture profile;
         * named P3 and its fixed-point ICC representation are not identical. */
        profile = fopen(argv[1], "rb");
        if (profile == NULL || fseek(profile, 0, SEEK_END) != 0) {
            goto end;
        }
        length = ftell(profile);
        if (length <= 0 || length > 16777216L ||
                fseek(profile, 0, SEEK_SET) != 0) {
            goto end;
        }
        bytes = malloc((size_t)length);
        if (bytes == NULL ||
                fread(bytes, 1, (size_t)length, profile) != (size_t)length) {
            goto end;
        }
        p3 = sixel_cdef_icc(bytes, (size_t)length);
    } else {
        goto end;
    }
    srgb = sixel_cdef_name("srgb");
    linear = sixel_cdef_linearized(srgb);
    if (p3 == NULL || srgb == NULL || linear == NULL ||
            SIXEL_FAILED(sixel_allocator_new(&allocator,
                NULL, NULL, NULL, NULL))) {
        goto end;
    }
    transform = sixel_cxform_new(p3, linear, 1);
    if (transform == NULL) {
        goto end;
    }
    for (i = 0; i < 3; ++i) {
        if (SIXEL_FAILED(sixel_gamut_new(&mapper[i], srgb,
                SIXEL_COLORSPACE_GAMMA, policies[i], allocator))) {
            goto end;
        }
    }

    while ((count = scanf("%lf %lf %lf", &input[0], &input[1],
                         &input[2])) == 3) {
        if (!sixel_cxform_apply(transform, input, values[0])) {
            goto end;
        }
        /* Clip is illustrated as a continuous output operation here.
         * Unlike an explicit mapper, the encoder's clip policy leaves
         * source samples untouched before palette construction. */
        for (j = 0; j < 3; ++j) {
            values[1][j] = fmax(0.0, fmin(1.0, values[0][j]));
        }
        for (i = 0; i < 3; ++i) {
            if (!sixel_gamut_apply(mapper[i], values[0], values[i + 2],
                    SIXEL_COLORSPACE_LINEAR)) {
                goto end;
            }
        }
        for (i = 0; i < 5; ++i) {
            if (!sixel_color_convert(lab, values[i],
                    SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB)) {
                goto end;
            }
            for (j = 0; j < 3; ++j) {
                printf("%.17g,", values[i][j]);
            }
            printf("%.17g,%.17g,%.17g%s", lab[0], lab[1], lab[2],
                i == 4 ? "\n" : ",");
        }
    }
    if (count == EOF && !ferror(stdin) && !ferror(stdout)) {
        result = EXIT_SUCCESS;
    }
end:
    if (profile != NULL) {
        fclose(profile);
    }
    free(bytes);
    for (i = 0; i < 3; ++i) {
        sixel_gamut_free(mapper[i]);
    }
    sixel_cxform_free(transform);
    sixel_cdef_free(linear);
    sixel_cdef_free(srgb);
    sixel_cdef_free(p3);
    sixel_allocator_unref(allocator);
    if (result != EXIT_SUCCESS) {
        fputs("gamut documentation probe failed\n", stderr);
    }
    return result;
}
