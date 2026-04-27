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

#ifndef LIBSIXEL_LOOKUP_POLICY_EYTZINGER_H
#define LIBSIXEL_LOOKUP_POLICY_EYTZINGER_H

#include "lookup-policy.h"

#ifdef __cplusplus
extern "C" {
#endif


/* @classid lookup/eytzinger.8bit */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_eytzinger_8bit_new(
    sixel_allocator_t *allocator,
    sixel_lookup_policy_interface_t **policy);

/* @classid lookup/eytzinger.float32 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_eytzinger_float32_new(
    sixel_allocator_t *allocator,
    sixel_lookup_policy_interface_t **policy);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_POLICY_EYTZINGER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
