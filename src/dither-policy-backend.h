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

#ifndef LIBSIXEL_DITHER_POLICY_BACKEND_H
#define LIBSIXEL_DITHER_POLICY_BACKEND_H

#include "dither-policy.h"
#include "dither-internal.h"
#include "pixelformat.h"

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_backend_make_effective_request(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_apply_request_t *effective);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_backend_build_context(
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_context_t *context,
    unsigned char scratch[SIXEL_MAX_CHANNELS],
    unsigned char new_palette[SIXEL_PALETTE_MAX * 4],
    float new_palette_float[SIXEL_PALETTE_MAX * SIXEL_MAX_CHANNELS],
    unsigned short migration_map[SIXEL_PALETTE_MAX]);

SIXEL_INTERNAL_API void
sixel_dither_policy_backend_ref(sixel_dither_policy_interface_t *policy);

SIXEL_INTERNAL_API void
sixel_dither_policy_backend_unref(sixel_dither_policy_interface_t *policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_backend_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_backend_create(
    sixel_dither_policy_interface_t **policy,
    sixel_dither_policy_vtbl_t const *vtbl);

#endif /* LIBSIXEL_DITHER_POLICY_BACKEND_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
