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

#ifndef LIBSIXEL_ENCODER_CORE_H
#define LIBSIXEL_ENCODER_CORE_H

#include <6cells.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The legacy public handle and the component interface share the same address
 * because the vtbl dispatch header is the first field of the private
 * sixel_output storage.
 */
SIXEL_INTERNAL_API sixel_encoder_core_t *
sixel_output_as_encoder_core(sixel_output_t const *output);

/* @classid codec/encoder-core */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_factory_new(sixel_allocator_t *allocator, void **object);

/* @classid codec/encoder-core.normal */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_normal_factory_new(sixel_allocator_t *allocator,
                                      void **object);

/* @classid codec/encoder-core.highcolor */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_highcolor_factory_new(sixel_allocator_t *allocator,
                                         void **object);

/* @classid codec/encoder-core.ormode */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_ormode_factory_new(sixel_allocator_t *allocator,
                                      void **object);

SIXEL_INTERNAL_API void
sixel_encoder_core_destroy(sixel_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_ENCODER_CORE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
