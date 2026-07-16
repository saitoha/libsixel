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
#include "config.h"
#endif

#include <stdlib.h>

#include "compat_stub.h"
#include "gpu-dequant.h"

#define SIXEL_GPU_DEQUANT_AUTO_THRESHOLD_DEFAULT 262144U
#define SIXEL_GPU_DEQUANT_THRESHOLD_ENVVAR \
    "SIXEL_GPU_DEQUANT_THRESHOLD"

#if defined(HAVE_METAL)
SIXELSTATUS
sixel_gpu_dequant_metal_fast4_rgba(
    sixel_gpu_dequant_request_t const *request);

int
sixel_gpu_dequant_metal_is_available(void);
#endif

static int
sixel_gpu_dequant_policy_is_force(int policy)
{
    return policy == SIXEL_GPU_POLICY_FORCE;
}

static size_t
sixel_gpu_dequant_auto_threshold(void)
{
    char const *text;
    char *endptr;
    unsigned long value;

    text = sixel_compat_getenv(SIXEL_GPU_DEQUANT_THRESHOLD_ENVVAR);
    endptr = NULL;
    value = 0UL;
    if (text == NULL || text[0] == '\0') {
        return (size_t)SIXEL_GPU_DEQUANT_AUTO_THRESHOLD_DEFAULT;
    }

    value = strtoul(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return (size_t)SIXEL_GPU_DEQUANT_AUTO_THRESHOLD_DEFAULT;
    }

    return (size_t)value;
}

static int
sixel_gpu_dequant_has_engine(void)
{
#if defined(HAVE_METAL)
    return sixel_gpu_dequant_metal_is_available();
#else
    return 0;
#endif
}

static int
sixel_gpu_dequant_request_is_supported(
    sixel_gpu_dequant_request_t const *request)
{
    size_t required_palette_size;

    required_palette_size = 0U;
    if (request == NULL || request->dest == NULL ||
            request->rgba == NULL || request->palette == NULL) {
        return 0;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGBA8888 ||
            request->width <= 0 || request->height <= 0 ||
            request->pixel_count == 0U) {
        return 0;
    }
    if (request->ncolors <= 0 || request->ncolors > SIXEL_PALETTE_MAX ||
            request->palette_depth != 3) {
        return 0;
    }
    required_palette_size =
        (size_t)request->ncolors * (size_t)request->palette_depth;
    if (request->palette_size < required_palette_size) {
        return 0;
    }

    return 1;
}

SIXELSTATUS
sixel_gpu_dequant_fast4_rgba(sixel_gpu_dequant_request_t const *request)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;
    if (request == NULL) {
        sixel_helper_set_additional_message(
            "gpu dequant: request is null.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->policy == SIXEL_GPU_POLICY_OFF) {
        return SIXEL_FALSE;
    }
    if (!sixel_gpu_dequant_request_is_supported(request)) {
        if (sixel_gpu_dequant_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu dequant: request shape is not supported.");
            return SIXEL_BAD_ARGUMENT;
        }
        return SIXEL_FALSE;
    }
    if (request->policy == SIXEL_GPU_POLICY_AUTO &&
            request->pixel_count < sixel_gpu_dequant_auto_threshold()) {
        return SIXEL_FALSE;
    }
    if (!sixel_gpu_dequant_has_engine()) {
        if (sixel_gpu_dequant_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu dequant: no GPU engine is available.");
            return SIXEL_FEATURE_ERROR;
        }
        return SIXEL_FALSE;
    }

#if defined(HAVE_METAL)
    status = sixel_gpu_dequant_metal_fast4_rgba(request);
    if (status == SIXEL_OK) {
        return SIXEL_OK;
    }
    if (sixel_gpu_dequant_policy_is_force(request->policy)) {
        if (status == SIXEL_FALSE) {
            sixel_helper_set_additional_message(
                "gpu dequant: GPU backend did not run.");
            return SIXEL_FEATURE_ERROR;
        }
        return status;
    }
#else
    status = SIXEL_FALSE;
#endif

    (void)status;
    return SIXEL_FALSE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
