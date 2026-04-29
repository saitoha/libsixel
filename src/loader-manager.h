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

#ifndef LIBSIXEL_LOADER_MANAGER_H
#define LIBSIXEL_LOADER_MANAGER_H

#include <sixel.h>

#include "chunk.h"
#include "logger.h"
#include "loader.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface ILoaderManager {
 *   ref();
 *   unref();
 *   build_chain(request);
 *   load(chunk, out selected_loader);
 * }
 */

typedef struct sixel_loader_entry {
    char const *name;
    char const *classid;
    int default_enabled;
} sixel_loader_entry_t;

typedef struct sixel_loader_suboptions {
    int wic_ico_minsize;
    int libjpeg_enable_cms;
    int libjpeg_cms_engine;
    int libjpeg_enable_orientation;
    int libpng_enable_cms;
    int libpng_cms_engine;
    int libpng_enable_orientation;
    int libwebp_enable_cms;
    int libwebp_cms_engine;
    int libwebp_enable_orientation;
    int coregraphics_enable_orientation;
    int libtiff_enable_cms;
    int libtiff_cms_engine;
    int builtin_enable_cms;
    int builtin_cms_engine;
    int builtin_enable_orientation;
    int builtin_bmp_info40_mode;
} sixel_loader_suboptions_t;

typedef struct sixel_loader_manager_interface sixel_loader_manager_t;

typedef struct sixel_loader_manager_build_request {
    sixel_option_argument_list_resolution_t const *resolution;
    int skip_predicate_gate;
    int require_static;
    int use_palette;
    int reqcolors;
    unsigned char const *bgcolor;
    int has_bgcolor;
    int bgcolor_source;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
    sixel_loader_suboptions_t const *suboptions;
    sixel_logger_t *timeline_logger;
    int *timeline_job_seq;
    sixel_loader_t *timeline_loader;
} sixel_loader_manager_build_request_t;

typedef struct sixel_loader_manager_vtbl {
    void (*ref)(sixel_loader_manager_t *manager);
    void (*unref)(sixel_loader_manager_t *manager);
    SIXELSTATUS (*build_chain)(
        sixel_loader_manager_t *manager,
        sixel_loader_manager_build_request_t const *request);
    SIXELSTATUS (*load)(
        sixel_loader_manager_t *manager,
        sixel_chunk_t const *chunk,
        sixel_loader_component_interface_t **selected_loader,
        sixel_load_image_function fn_load,
        void *load_context);
} sixel_loader_manager_vtbl_t;

struct sixel_loader_manager_interface {
    sixel_loader_manager_vtbl_t const *vtbl;
};

/* @classid loader/manager */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_manager_new(sixel_allocator_t *allocator,
                         void **manager);

SIXELSTATUS
loader_manager_parse_loader_order(
    char const *order,
    sixel_option_argument_list_resolution_t *resolution);

void
loader_manager_init_loader_suboptions(
    sixel_loader_suboptions_t *suboptions);

void
loader_manager_resolve_loader_suboptions(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_suboptions_t *suboptions);

size_t
loader_manager_build_plan_from_resolution(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity);

SIXEL_INTERNAL_API size_t
loader_manager_get_entries(sixel_loader_entry_t const **entries);

SIXEL_INTERNAL_API int
loader_manager_entry_available(char const *name);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOADER_MANAGER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
