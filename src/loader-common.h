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
#include "logger.h"

#define SIXEL_THUMBNAILER_DEFAULT_SIZE 512

typedef struct sixel_loader_timeline_callback_state {
    unsigned int magic;
    sixel_load_image_function fn_load;
    void *context;
    int header_job_id;
    int header_closed;
    int decode_job_id;
    int decode_open;
} sixel_loader_timeline_callback_state_t;

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
#define SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE           (0x10007)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBJPEG_ENABLE_ORIENTATION (0x10008)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBPNG_ENABLE_ORIENTATION  (0x10009)
#define SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_ORIENTATION (0x1000a)
#define SIXEL_LOADER_COMPONENT_OPTION_COREGRAPHICS_ENABLE_ORIENTATION \
    (0x1000b)
#define SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_BMP_INFO40_MODE \
    (0x1000c)
#define SIXEL_LOADER_COMPONENT_OPTION_BGCOLOR_SOURCE \
    (0x1000d)
#define SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_ENABLE_ORIENTATION \
    (0x1000e)

#define SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO    0
#define SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS 1
#define SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2     2

#define SIXEL_LOADER_TRANSPARENT_POLICY_COMPOSITE  0
#define SIXEL_LOADER_TRANSPARENT_POLICY_TRANSPARENT 1

#define SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST     0
#define SIXEL_LOADER_BACKGROUND_POLICY_EXPLICIT_FIRST 1

void loader_thumbnailer_initialize_size_hint(void);
int loader_thumbnailer_get_size_hint(void);
int loader_thumbnailer_get_default_size_hint(void);
SIXEL_INTERNAL_API void sixel_helper_set_thumbnail_size_hint(int size);
int loader_wic_get_ico_minsize(void);
void sixel_helper_set_wic_ico_minsize(int size);
int loader_libpng_get_enable_cms(void);
void sixel_helper_set_libpng_enable_cms(int enable);
int loader_builtin_get_enable_cms(void);
void sixel_helper_set_builtin_enable_cms(int enable);
/*
 * Keep these exported: test runners call them through the shared libsixel
 * library on Windows.
 */
SIXEL_INTERNAL_API void
sixel_helper_set_loader_background_colorspace(int colorspace);
SIXEL_INTERNAL_API int
loader_background_colorspace(void);
int loader_cms_prefer_8bit(void);
int loader_cms_target_colorspace(void);
int loader_transparent_policy(void);
/*
 * Keep this exported: the test runner references this helper via the shared
 * libsixel DLL on Windows builds.
 */
SIXEL_INTERNAL_API int loader_background_policy(void);
/* Keep this exported: test runners call it via the shared libsixel DLL. */
SIXEL_INTERNAL_API int loader_cms_target_pixelformat(void);
void sixel_helper_set_loader_cms_engine(int engine);
SIXEL_INTERNAL_API void sixel_helper_set_loader_trace(int enable);
void loader_trace_try(char const *name);
void loader_trace_result(char const *name, SIXELSTATUS status);
int loader_trace_is_enabled(void);
void loader_trace_message(char const *format, ...);
int sixel_trace_topic_is_enabled(char const *topic);
void sixel_trace_topic_message(char const *topic,
                               char const *format, ...);

void loader_timeline_scope_begin(sixel_logger_t *logger,
                                 char const *worker,
                                 int *job_seq);
void loader_timeline_scope_end(void);
int loader_timeline_phase_start(char const *role);
void loader_timeline_phase_finish(char const *role,
                                  int job_id,
                                  SIXELSTATUS status);
void loader_timeline_optional_mark(char const *role);
void loader_timeline_optional_skip_if_unmarked(char const *role);
void loader_timeline_callback_state_init(
    sixel_loader_timeline_callback_state_t *state,
    sixel_load_image_function fn_load,
    void *context,
    int header_job_id,
    int decode_job_id);
void loader_timeline_callback_close_header(
    sixel_loader_timeline_callback_state_t *state,
    SIXELSTATUS status);
void loader_timeline_callback_close_decode(
    sixel_loader_timeline_callback_state_t *state,
    SIXELSTATUS status);
void *loader_timeline_unwrap_callback_context(void *context);
SIXELSTATUS loader_timeline_emit_frame_callback(sixel_frame_t *frame,
                                                void *data);

int chunk_is_png(sixel_chunk_t const *chunk);
int chunk_is_jpeg(sixel_chunk_t const *chunk);
int chunk_is_webp(sixel_chunk_t const *chunk);
int chunk_is_bmp(sixel_chunk_t const *chunk);
int chunk_is_gif(sixel_chunk_t const *chunk);
int chunk_is_tiff(sixel_chunk_t const *chunk);
int chunk_is_wbmp(sixel_chunk_t const *chunk);
int chunk_is_tga(sixel_chunk_t const *chunk);
int chunk_is_gd(sixel_chunk_t const *chunk);
int chunk_is_gd2(sixel_chunk_t const *chunk);

int loader_exif_parse_orientation(unsigned char const *data,
                                  size_t size,
                                  int *orientation);
SIXELSTATUS loader_frame_apply_orientation(sixel_frame_t *frame,
                                           int orientation);

#endif /* LIBSIXEL_LOADER_COMMON_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
