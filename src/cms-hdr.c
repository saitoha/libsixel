/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "cms-hdr.h"
#include "compat_stub.h"

/* BT.2100 transfer decoding is kept separate from the output tone policy.
 * PQ returns absolute cd/m2; HLG first returns scene-relative light. */
static int
sixel_cms_hdr_decode(double encoded, int transfer, double *light)
{
    double power;
    double numerator;
    double denominator;

    if (!isfinite(encoded) || encoded < 0.0 || encoded > 1.0 ||
        light == NULL) {
        return 0;
    }
    if (transfer == SIXEL_CMS_HDR_PQ) {
        power = pow(encoded, 32.0 / 2523.0);
        numerator = fmax(power - 3424.0 / 4096.0, 0.0);
        denominator = 2413.0 / 128.0 - 2392.0 / 128.0 * power;
        *light = 10000.0 * pow(numerator / denominator, 16384.0 / 2610.0);
    } else if (transfer == SIXEL_CMS_HDR_HLG) {
        *light = encoded <= 0.5 ? encoded * encoded / 3.0
            : (exp((encoded - 0.55991073) / 0.17883277) + 0.28466892)
                / 12.0;
    } else {
        return 0;
    }
    return isfinite(*light);
}

int
sixel_cms_hdr_to_linear(double output[3], double const input[3],
                         sixel_cms_hdr_t const *hdr)
{
    /* D65 RGB-to-sRGB matrices derived from the published xy primaries.
     * No PCS chromatic adaptation is needed: all three bases use D65. */
    static double const matrices[3][3][3] = {
        {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},
        {{1.22494017628056, -0.22494017628056, 0.0},
         {-0.04205695470969, 1.04205695470969, 0.0},
         {-0.01963755459033, -0.07863604555063, 1.09827360014096}},
        {{1.66049100210843, -0.58764113878855, -0.07284986331988},
         {-0.12455047452159, 1.13289989712596, -0.00834942260437},
         {-0.01815076335491, -0.10057889800801, 1.11872966136292}}
    };
    static double const luminance[3][3] = {
        {0.21263900587151, 0.71516867876776, 0.07219231536073},
        {0.22897456406975, 0.69173852183651, 0.07928691409375},
        {0.26270021201127, 0.67799807151887, 0.05930171646986}
    };
    double linear[3];
    double result[3];
    double scale;
    double y;
    double gamma;
    size_t i;
    size_t j;

    if (output == NULL || input == NULL || hdr == NULL ||
        hdr->primaries < 0 || hdr->primaries > 2 ||
        !isfinite(hdr->reference_white) || hdr->reference_white <= 0.0 ||
        !isfinite(hdr->peak_luminance) || hdr->peak_luminance < 400.0 ||
        hdr->peak_luminance > 10000.0) {
        return 0;
    }
    y = 0.0;
    for (i = 0u; i < 3u; ++i) {
        if (!sixel_cms_hdr_decode(input[i], hdr->transfer, &linear[i])) {
            return 0;
        }
        y += linear[i] * luminance[hdr->primaries][i];
    }
    scale = 1.0 / hdr->reference_white;
    if (hdr->transfer == SIXEL_CMS_HDR_HLG) {
        /* Reference display OOTF, zero display black. The common luminance
         * factor must be applied before changing primaries, not as three
         * independent channel powers. */
        gamma = 1.2 + 0.42 * log10(hdr->peak_luminance / 1000.0);
        scale *= y > 0.0 ? hdr->peak_luminance * pow(y, gamma - 1.0) : 0.0;
    }
    for (i = 0u; i < 3u; ++i) {
        result[i] = 0.0;
        for (j = 0u; j < 3u; ++j) {
            result[i] += matrices[hdr->primaries][i][j] * linear[j];
        }
        result[i] *= scale;
        if (!isfinite(result[i])) {
            return 0;
        }
    }
    memcpy(output, result, sizeof(result));
    return 1;
}

int
sixel_cms_hdr_convert(float *pixels, size_t count,
                       sixel_cms_hdr_t const *hdr)
{
    float *result;
    double input[3];
    double output[3];
    size_t i;
    size_t channel;
    int ok;

    if (pixels == NULL || count == 0u ||
        count > (size_t)-1 / (3u * sizeof(float))) {
        return 0;
    }
    result = malloc(count * 3u * sizeof(float));
    if (result == NULL) {
        return 0;
    }
    ok = 0;
    for (i = 0u; i < count; ++i) {
        for (channel = 0u; channel < 3u; ++channel) {
            input[channel] = pixels[i * 3u + channel];
        }
        if (!sixel_cms_hdr_to_linear(output, input, hdr)) {
            goto cleanup;
        }
        for (channel = 0u; channel < 3u; ++channel) {
            if (fabs(output[channel]) > FLT_MAX) {
                goto cleanup;
            }
            result[i * 3u + channel] = (float)output[channel];
        }
    }
    memcpy(pixels, result, count * 3u * sizeof(float));
    ok = 1;
cleanup:
    free(result);
    return ok;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
