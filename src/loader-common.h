/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Shared loader helpers accessed by backend-specific implementations.  The
 * declarations here keep cross-module dependencies small while allowing
 * backends to reuse detection and thumbnailing utilities.
 */

#ifndef LIBSIXEL_LOADER_COMMON_H
#define LIBSIXEL_LOADER_COMMON_H

#include <sixel.h>

#include "chunk.h"

#define SIXEL_THUMBNAILER_DEFAULT_SIZE 512

/*
 * Internal loader-component options used by the manager to pass resolved
 * -L suboption values without relying on process-global mutable state.
 */
#define SIXEL_LOADER_COMPONENT_OPTION_WIC_ICO_MINSIZE      (0x10001)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBPNG_ENABLE_CMS    (0x10002)
#define SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_ENABLE_CMS   (0x10003)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBJPEG_ENABLE_CMS   (0x10004)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS   (0x10005)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_CMS   (0x10006)

void loader_thumbnailer_initialize_size_hint(void);
int loader_thumbnailer_get_size_hint(void);
int loader_thumbnailer_get_default_size_hint(void);
void sixel_helper_set_thumbnail_size_hint(int size);
int loader_wic_get_ico_minsize(void);
void sixel_helper_set_wic_ico_minsize(int size);
int loader_libpng_get_enable_cms(void);
void sixel_helper_set_libpng_enable_cms(int enable);
int loader_builtin_get_enable_cms(void);
void sixel_helper_set_builtin_enable_cms(int enable);
int loader_background_colorspace(void);
int loader_cms_prefer_8bit(void);
int loader_cms_target_colorspace(void);
int loader_cms_target_pixelformat(void);
void sixel_helper_set_loader_trace(int enable);
void loader_trace_try(char const *name);
void loader_trace_result(char const *name, SIXELSTATUS status);
int loader_trace_is_enabled(void);
void loader_trace_message(char const *format, ...);
int sixel_trace_topic_is_enabled(char const *topic);
void sixel_trace_topic_message(char const *topic,
                               char const *format, ...);

int chunk_is_png(sixel_chunk_t const *chunk);
int chunk_is_jpeg(sixel_chunk_t const *chunk);
int chunk_is_webp(sixel_chunk_t const *chunk);
int chunk_is_bmp(sixel_chunk_t const *chunk);
int chunk_is_gif(sixel_chunk_t const *chunk);
int chunk_is_tiff(sixel_chunk_t const *chunk);

#endif /* LIBSIXEL_LOADER_COMMON_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
