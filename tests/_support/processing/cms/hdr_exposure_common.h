/* SPDX-License-Identifier: MIT */
#ifndef CMS_HDR_EXPOSURE_COMMON_H
#define CMS_HDR_EXPOSURE_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/compat_stub.h"
#include "src/loader-common.h"

static int
cms_hdr_exposure_test(int reinhard, int signed_colors)
{
    static int const evs[11] = {
        -1074, -1023, -149, -24, -1, 0, 1, 24, 149, 1000, 1023
    };
    static float const samples[6][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.000001f, 0.000001f, 0.000001f},
        {49.2610837f, 49.2610837f, 49.2610837f},
        {81.7975863f, -6.13549136f, -0.89412627f},
        {-28.948f, 55.811f, -4.955f},
        {0.01f, 0.2f, 5.0f}
    };
    float pixel[3];
    char ev[32];
    double luminance;
    double z;
    double denominator_log;
    double reference;
    double tolerance;
    size_t e;
    size_t s;
    size_t c;
    size_t first;
    size_t end;
    size_t ev_count;

    first = signed_colors ? 3u : 0u;
    end = first + 3u;
    ev_count = signed_colors ? 11u : 5u;
    if (sixel_compat_setenv("SIXEL_LOADER_HDR_TONEMAP",
            reinhard ? "reinhard" : "auto") != 0) {
        return 1;
    }
    for (e = 0u; e < ev_count; ++e) {
        (void)sixel_compat_snprintf(ev, sizeof(ev), "%d", evs[e]);
        if (sixel_compat_setenv("SIXEL_LOADER_HDR_EXPOSURE_EV", ev) != 0) {
            return 1;
        }
        for (s = first; s < end; ++s) {
            memcpy(pixel, samples[s], sizeof(pixel));
            if (!loader_hdr_finish(pixel, 1u)) {
                fprintf(stderr, "HDR rejected finite EV %d\n", evs[e]);
                return 1;
            }
            luminance = 0.21263900587151036 * samples[s][0] +
                0.71516867876775593 * samples[s][1] +
                0.07219231536073371 * samples[s][2];
            /* Independent log-domain evaluation avoids the production
             * multiply/reciprocal branches and spans subnormal exposure.
             * Negative color is preserved by auto, clipped by reinhard. */
            for (c = 0u; c < 3u; ++c) {
                reference = 0.0;
                if (reinhard && samples[s][c] > 0.0f) {
                    z = log(samples[s][c]) + evs[e] * log(2.0);
                    reference = z > 0.0 ? 1.0 / (1.0 + exp(-z))
                        : exp(z) / (1.0 + exp(z));
                } else if (!reinhard && samples[s][c] != 0.0f) {
                    z = log(luminance) + evs[e] * log(2.0);
                    denominator_log = z > 0.0 ? z + log1p(exp(-z))
                                              : log1p(exp(z));
                    reference = copysign(exp(log(fabs(samples[s][c])) +
                        evs[e] * log(2.0) - denominator_log), samples[s][c]);
                }
                tolerance = 1e-6 * fabs(reference) +
                    ldexp(1.0, FLT_MIN_EXP - FLT_MANT_DIG);
                if (!isfinite(pixel[c]) ||
                    fabs(pixel[c] - reference) > tolerance) {
                    fprintf(stderr, "EV=%d sample=%zu channel=%zu: "
                        "%.12g != %.12g\n", evs[e], s, c,
                        (double)pixel[c], reference);
                    return 1;
                }
            }
        }
    }
    return 0;
}
#endif
