/* SPDX-License-Identifier: MIT */
#ifndef CMS_HDR_NUMERIC_COMMON_H
#define CMS_HDR_NUMERIC_COMMON_H
#include <math.h>
#include <string.h>
#include "src/cms-hdr.h"

static int
cms_hdr_numeric_test(int mode)
{
    /* Neutral values were evaluated with 70-digit decimal arithmetic from
     * BT.2100's rational PQ constants and published HLG constants. Colored
     * values additionally use independently solved xy primary matrices;
     * no production transfer or matrix helper computes these expectations. */
    static double const pq[5] = {
        0.0, 0.0253900296050889289, 0.4544123595766703413,
        4.8442258895916144224, 49.261083743842364532
    };
    static double const hlg[5] = {
        0.0, 0.0473167031753760142, 0.2497390566064063051,
        1.0007494845008182846, 4.9261085184233034119
    };
    static double const colored[3][3] = {
        {0.291032543023254445, 0.0299959091274250581, 0.797705407906724909},
        {81.7975863107603155, -6.13549135574338622, -0.894126273640655349},
        {60.3418806049536921, -2.07177116796493403, -0.967367221198740547}
    };
    sixel_cms_hdr_t hdr = {SIXEL_CMS_HDR_PQ, 2, 203.0, 1000.0};
    double input[3] = {0.5, 0.5, 0.5};
    double output[3] = {-99.0, -99.0, -99.0};
    double original[3];
    double expected[3];
    float pixels[6] = {0.5f, 0.5f, 0.5f, 0.75f, 0.75f, 0.75f};
    float saved[6];
    size_t i;
    size_t c;
    int ok;

    ok = 1;
    if (mode < 2) {
        hdr.transfer = mode ? SIXEL_CMS_HDR_HLG : SIXEL_CMS_HDR_PQ;
        for (i = 0u; i < 5u; ++i) {
            for (c = 0u; c < 3u; ++c) { input[c] = i / 4.0; }
            ok = ok && sixel_cms_hdr_to_linear(output, input, &hdr);
            for (c = 0u; c < 3u; ++c) {
                ok = ok && isfinite(output[c]) &&
                    fabs(output[c] - (mode ? hlg[i] : pq[i])) < 2e-10;
            }
        }
    } else if (mode < 5) {
        hdr.transfer = mode == 2 ? SIXEL_CMS_HDR_HLG : SIXEL_CMS_HDR_PQ;
        hdr.primaries = mode == 4 ? 1 : 2;
        input[0] = mode == 2 ? 0.5 : 1.0;
        input[1] = mode == 2 ? 0.25 : 0.0;
        input[2] = mode == 2 ? 0.75 : 0.0;
        ok = sixel_cms_hdr_to_linear(output, input, &hdr);
        for (c = 0u; c < 3u; ++c) {
            ok = ok && isfinite(output[c]) &&
                fabs(output[c] - colored[mode - 2][c]) < 2e-10;
        }
    } else if (mode == 5) {
        /* An invalid late sample cannot publish an earlier decoded pixel. */
        pixels[5] = NAN;
        memcpy(saved, pixels, sizeof(saved));
        ok = !sixel_cms_hdr_convert(pixels, 2u, &hdr) &&
            memcmp(saved, pixels, sizeof(saved)) == 0;
        pixels[5] = 0.75f;
        ok = ok && sixel_cms_hdr_convert(pixels, 2u, &hdr);
        for (i = 0u; i < 6u; ++i) {
            ok = ok && fabs(pixels[i] - pq[i < 3u ? 2 : 3]) < 3e-7;
        }
    } else if (mode == 6) {
        memcpy(original, output, sizeof(original));
        hdr.reference_white = 0.0;
        ok = !sixel_cms_hdr_to_linear(output, input, &hdr) &&
            memcmp(output, original, sizeof(output)) == 0;
        hdr.reference_white = 203.0;
        hdr.peak_luminance = NAN;
        ok = ok && !sixel_cms_hdr_to_linear(output, input, &hdr) &&
            memcmp(output, original, sizeof(output)) == 0;
        hdr.peak_luminance = 1000.0;
        input[2] = -0.01;
        ok = ok && !sixel_cms_hdr_to_linear(output, input, &hdr) &&
            memcmp(output, original, sizeof(output)) == 0;
        input[2] = 1.01;
        ok = ok && !sixel_cms_hdr_to_linear(output, input, &hdr) &&
            memcmp(output, original, sizeof(output)) == 0;
    } else if (mode == 7) {
        memcpy(saved, pixels, sizeof(saved));
        ok = !sixel_cms_hdr_convert(pixels, (size_t)-1, &hdr) &&
            memcmp(saved, pixels, sizeof(saved)) == 0;
    } else {
        hdr.reference_white = 100.0;
        hdr.primaries = 0;
        expected[0] = expected[1] = expected[2] = 0.922457089940640;
        ok = sixel_cms_hdr_to_linear(input, input, &hdr);
        for (c = 0u; c < 3u; ++c) {
            ok = ok && fabs(input[c] - expected[c]) < 2e-10;
        }
    }
    return ok ? 0 : 1;
}
#endif
