/* SPDX-License-Identifier: MIT */
#ifndef CMS_HDR_FINISH_COMMON_H
#define CMS_HDR_FINISH_COMMON_H
#include <math.h>
#include <string.h>
#include "src/loader-common.h"

static int
cms_hdr_finish_test(int mode)
{
    float pixels[3] = {49.2610837f, 49.2610837f, 49.2610837f};
    float saved[3];
    double expected[3] = {1.0, 1.0, 1.0};
    size_t c;
    int ok;

    if (mode == 3) {
        pixels[0] = -1.0f;
        pixels[1] = 0.25f;
        pixels[2] = 3.0f;
        expected[0] = 0.0;
        expected[1] = 0.2;
        expected[2] = 0.75;
    }
    memcpy(saved, pixels, sizeof(saved));
    ok = loader_hdr_finish(pixels, 1u);
    if (mode == 2) {
        return !ok && memcmp(saved, pixels, sizeof(saved)) == 0 ? 0 : 1;
    }
    /* With finite 2^1023 exposure, either neutral Reinhard formulation
     * tends to one. No production formula computes the expected value. */
    for (c = 0u; ok && c < 3u; ++c) {
        ok = isfinite(pixels[c]) && fabs(pixels[c] - expected[c]) < 1e-7;
    }
    return ok ? 0 : 1;
}
#endif
