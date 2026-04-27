/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FROMWEBP_CONTAINER_H
#define LIBSIXEL_FROMWEBP_CONTAINER_H

#include "fromwebp-internal.h"

/*
 * Container parser entrypoints are declared here so fromwebp-container.c
 * keeps an explicit source/header pairing like other WebP internal modules.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_parse_container(sixel_chunk_t const *chunk,
                           sixel_webp_container_info_t *info);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_webp_build_decode_plan(sixel_webp_container_info_t const *info,
                             sixel_webp_decode_plan_t *plan);

SIXEL_INTERNAL_API sixel_webp_container_kind_t
sixel_webp_classify_container(sixel_webp_container_info_t const *info);

#endif  /* LIBSIXEL_FROMWEBP_CONTAINER_H */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
