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

#include <string.h>

#include "dither-policy-sierra2.h"
#include "dither-policy-backend.h"
#include "dither-fixed-8bit.h"
#include "dither-fixed-float32.h"
#include "pixelformat.h"

/*
 * IDL (internal contract)
 *
 * class DitherPolicy : IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 */

static SIXELSTATUS
sixel_dither_policy_sierra2_apply(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_apply_request_t const *request)
{
    SIXELSTATUS status;
    sixel_dither_policy_apply_request_t effective;
    sixel_dither_context_t context;
    unsigned char scratch[SIXEL_MAX_CHANNELS];
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4];
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS];
    unsigned short migration_map[SIXEL_PALETTE_MAX];

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));
    memset(scratch, 0, sizeof(scratch));
    memset(new_palette, 0, sizeof(new_palette));
    memset(new_palette_float, 0, sizeof(new_palette_float));
    memset(migration_map, 0, sizeof(migration_map));

    status = sixel_dither_policy_backend_make_effective_request(policy,
                                                                request,
                                                                &effective);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_dither_policy_backend_build_context(&effective,
                                                       &context,
                                                       scratch,
                                                       new_palette,
                                                       new_palette_float,
                                                       migration_map);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    context.method_for_diffuse = SIXEL_DIFFUSE_SIERRA2;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(context.pixelformat)
            && context.pixels_float != NULL
            && context.depth == 3
            && effective.dither != NULL
            && effective.dither->prefer_float32 != 0) {
        status = sixel_dither_apply_fixed_float32(effective.dither, &context);
        if (status == SIXEL_BAD_ARGUMENT) {
            status = sixel_dither_apply_fixed_8bit(effective.dither, &context);
        }
    } else {
        status = sixel_dither_apply_fixed_8bit(effective.dither, &context);
    }

    return status;
}

static sixel_dither_policy_supports_parallel_result_t
sixel_dither_policy_sierra2_supports_parallel_bands(
    sixel_dither_policy_interface_t const *policy)
{
    (void)policy;
    return 1;
}

static sixel_dither_policy_vtbl_t const g_sixel_dither_policy_sierra2_vtbl = {
    sixel_dither_policy_backend_ref,
    sixel_dither_policy_backend_unref,
    sixel_dither_policy_backend_prepare,
    sixel_dither_policy_sierra2_apply,
    sixel_dither_policy_sierra2_supports_parallel_bands
};

SIXELSTATUS
sixel_dither_policy_create_sierra2(
    sixel_dither_policy_interface_t **policy)
{
    return sixel_dither_policy_backend_create(
        policy,
        &g_sixel_dither_policy_sierra2_vtbl);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
