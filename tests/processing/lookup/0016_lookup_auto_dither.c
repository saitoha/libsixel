/* SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * AUTO keeps exact lookup while selecting bulk versus causal point mapping.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/lookup-policy.h"

int
test_lookup_0016_auto_dither(int argc, char **argv)
{
    sixel_lookup_policy_select_request_t request;
    int methods[] = { SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_FS,
                      SIXEL_DIFFUSE_LSO2, SIXEL_DIFFUSE_X_DITHER,
                      SIXEL_DIFFUSE_AUTO };
    int i;
    int f;
    int explicit_policy;
    char const *name;

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    request.depth = 3;
    request.reqcolor = 256;
    request.optimize_lookup = 1;
    for (f = 0; f < 2; ++f) {
        request.pixelformat = f ? SIXEL_PIXELFORMAT_RGBFLOAT32
                               : SIXEL_PIXELFORMAT_RGB888;
        for (i = 0; i < 5; ++i) {
            request.method_for_diffuse = methods[i];
            if (sixel_lookup_policy_select_batch(methods[i]) != (i == 0)) {
                return EXIT_FAILURE;
            }
            for (explicit_policy = 0; explicit_policy < 2; ++explicit_policy) {
                request.lut_policy = explicit_policy ? SIXEL_LUT_POLICY_VPTREE
                                                    : SIXEL_LUT_POLICY_AUTO;
                name = sixel_lookup_policy_select_name_with_override(
                    &request, explicit_policy);
                if (strcmp(name, f ? "lookup/vptree.float32"
                                   : "lookup/vptree.8bit") != 0) {
                    return EXIT_FAILURE;
                }
            }
            request.lut_policy = SIXEL_LUT_POLICY_6BIT;
            name = sixel_lookup_policy_select_name_with_override(&request, 1);
            if (strcmp(name, f ? "lookup/6bit.float32"
                               : "lookup/6bit.8bit") != 0) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
