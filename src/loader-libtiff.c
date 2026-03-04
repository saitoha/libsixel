/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 * libtiff-backed loader helpers. The implementation stays close to other
 * loader backends so the rest of libsixel continues to operate on decoded
 * RGBA buffers and consistent metadata.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBTIFF

#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <tiffio.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-libtiff.h"
#include "logger.h"

typedef struct sixel_loader_libtiff_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_libtiff_component_t;

typedef struct tiff_memory_chunk {
    unsigned char const *buffer;
    toff_t size;
    toff_t offset;
} tiff_memory_chunk_t;

#if HAVE_LCMS2
/*
 * Convert decoded RGBA pixels from an embedded TIFF ICC profile to sRGB.
 *
 * The alpha channel is preserved while only color channels are transformed.
 * Invalid or unsupported profiles are ignored to preserve compatibility.
 */
static void
tiff_convert_embedded_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  void const *profile,
                                  uint32_t profile_length)
{
    sixel_cms_profile_t * src_profile;
    sixel_cms_profile_t * dst_profile;
    sixel_cms_transform_t * transform;
    size_t pixel_count;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    pixel_count = 0;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }
    transform = sixel_cms_create_transform(src_profile,
                                   SIXEL_CMS_PIXELFORMAT_RGBA_8,
                                   dst_profile,
                                   SIXEL_CMS_PIXELFORMAT_RGBA_8,
                                   SIXEL_CMS_TRANSFORM_COPY_ALPHA);
    if (transform == NULL) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    sixel_cms_do_transform(transform, pixels, pixels, pixel_count);

cleanup:
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    if (src_profile != NULL) {
        sixel_cms_close_profile(src_profile);
    }
}
#endif

static tsize_t
tiff_memory_read(thandle_t handle, tdata_t data, tsize_t length)
{
    tiff_memory_chunk_t *chunk;
    tsize_t to_copy;
    toff_t available;

    chunk = (tiff_memory_chunk_t *)handle;
    if (chunk->offset >= chunk->size) {
        return 0;
    }

    available = chunk->size - chunk->offset;
    to_copy = length;
    if ((toff_t)to_copy > available) {
        to_copy = (tsize_t)available;
    }

    if (to_copy > 0) {
        memcpy(data, chunk->buffer + chunk->offset, to_copy);
        chunk->offset += (toff_t)to_copy;
    }

    return to_copy;
}

static tsize_t
tiff_memory_write(thandle_t handle, tdata_t data, tsize_t length)
{
    (void)handle;
    (void)data;
    (void)length;

    return 0;
}

static toff_t
tiff_memory_seek(thandle_t handle, toff_t offset, int whence)
{
    tiff_memory_chunk_t *chunk;
    toff_t new_offset;

    chunk = (tiff_memory_chunk_t *)handle;
    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        if (offset > chunk->size - chunk->offset) {
            new_offset = chunk->size;
        } else {
            new_offset = chunk->offset + offset;
        }
        break;
    case SEEK_END:
        if (offset > chunk->size) {
            new_offset = 0;
        } else {
            new_offset = chunk->size - offset;
        }
        break;
    default:
        return (toff_t)-1;
    }

    if (new_offset > chunk->size) {
        new_offset = chunk->size;
    }

    chunk->offset = new_offset;
    return chunk->offset;
}

static int
tiff_memory_close(thandle_t handle)
{
    (void)handle;
    return 0;
}

static toff_t
tiff_memory_size(thandle_t handle)
{
    tiff_memory_chunk_t *chunk;

    chunk = (tiff_memory_chunk_t *)handle;
    return chunk->size;
}

static int
tiff_memory_map(thandle_t handle, tdata_t *data, toff_t *size)
{
    (void)handle;
    (void)data;
    (void)size;

    return 0;
}

static void
tiff_memory_unmap(thandle_t handle, tdata_t data, toff_t size)
{
    (void)handle;
    (void)data;
    (void)size;
}

/*
 * Decode a TIFF stream into an RGBA buffer.
 *
 * The memory-backed TIFF client uses the following flow:
 *
 *    +-----------+     +-----------------+     +--------------------+
 *    | TIFF data | --> | libtiff decode | --> | RGBA pixel buffer  |
 *    +-----------+     +-----------------+     +--------------------+
 */
static SIXELSTATUS
load_tiff(unsigned char      /* out */ **result,
          unsigned char      /* in */  *buffer,
          size_t             /* in */  size,
          int                /* out */ *pwidth,
          int                /* out */ *pheight,
          int                /* out */ *ppixelformat,
          sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    TIFF *tif;
    tiff_memory_chunk_t chunk;
    uint32_t width;
    uint32_t height;
    uint32_t *raster;
    size_t pixel_count;
    size_t pixel_bytes;
    size_t index;
    unsigned char *pixels;
    uint32_t pixel;
    size_t offset;
#if HAVE_LCMS2
    uint32_t icc_profile_length;
    void *icc_profile;
#endif

    status = SIXEL_TIFF_ERROR;
    tif = NULL;
    raster = NULL;
    pixels = NULL;
    width = 0;
    height = 0;
    pixel_count = 0;
    pixel_bytes = 0;
#if HAVE_LCMS2
    icc_profile_length = 0u;
    icc_profile = NULL;
#endif

    chunk.buffer = buffer;
    chunk.size = (toff_t)size;
    chunk.offset = 0;

    tif = TIFFClientOpen("sixel-memory",
                         "r",
                         (thandle_t)&chunk,
                         tiff_memory_read,
                         tiff_memory_write,
                         tiff_memory_seek,
                         tiff_memory_close,
                         tiff_memory_size,
                         tiff_memory_map,
                         tiff_memory_unmap);
    if (tif == NULL) {
        sixel_helper_set_additional_message(
            "load_tiff: TIFFClientOpen() failed.");
        goto cleanup;
    }

    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height)) {
        sixel_helper_set_additional_message(
            "load_tiff: missing image dimensions.");
        goto cleanup;
    }

    if (width == 0 || height == 0) {
        sixel_helper_set_additional_message(
            "load_tiff: invalid image dimensions.");
        goto cleanup;
    }

    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_count = (size_t)width * (size_t)height;

    if (pixel_count > SIZE_MAX / sizeof(uint32_t)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    raster = (uint32_t *)sixel_allocator_malloc(
        allocator,
        pixel_count * sizeof(uint32_t));
    if (raster == NULL) {
        sixel_helper_set_additional_message(
            "load_tiff: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    if (!TIFFReadRGBAImageOriented(
            tif, width, height, raster, ORIENTATION_TOPLEFT, 0)) {
        sixel_helper_set_additional_message(
            "load_tiff: TIFFReadRGBAImageOriented() failed.");
        goto cleanup;
    }

    if (pixel_count > SIZE_MAX / 4) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_bytes = pixel_count * 4;

    pixels = (unsigned char *)sixel_allocator_malloc(allocator, pixel_bytes);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_tiff: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (index = 0; index < pixel_count; ++index) {
        pixel = raster[index];
        offset = index * 4;
        pixels[offset + 0] = TIFFGetR(pixel);
        pixels[offset + 1] = TIFFGetG(pixel);
        pixels[offset + 2] = TIFFGetB(pixel);
        pixels[offset + 3] = TIFFGetA(pixel);
    }

#if HAVE_LCMS2
    if (TIFFGetField(tif,
                     TIFFTAG_ICCPROFILE,
                     &icc_profile_length,
                     &icc_profile) == 1) {
        tiff_convert_embedded_icc_to_srgb(pixels,
                                          (int)width,
                                          (int)height,
                                          icc_profile,
                                          icc_profile_length);
    }
#endif

    *result = pixels;
    *pwidth = (int)width;
    *pheight = (int)height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = SIXEL_OK;

cleanup:
    if (raster != NULL) {
        sixel_allocator_free(allocator, raster);
    }
    if (status != SIXEL_OK && pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
        pixels = NULL;
        *result = NULL;
    }
    if (tif != NULL) {
        TIFFClose(tif);
    }

    return status;
}

/*
 * Dedicated libtiff loader wiring for the common loader callbacks.
 */
static SIXELSTATUS
load_with_libtiff(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_tiff(&pixels,
                       pchunk->buffer,
                       pchunk->size,
                       &frame->width,
                       &frame->height,
                       &frame->pixelformat,
                       pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_set_pixels(frame, pixels);

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


static void
sixel_loader_libtiff_ref(sixel_loader_component_t *component)
{
    sixel_loader_libtiff_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_libtiff_unref(sixel_loader_component_t *component)
{
    sixel_loader_libtiff_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_libtiff_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_libtiff_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_libtiff_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_libtiff_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_libtiff(chunk,
                             self->fstatic,
                             self->fuse_palette,
                             self->reqcolors,
                             bgcolor,
                             self->loop_control,
                             self->has_start_frame_no,
                             self->start_frame_no,
                             fn_load,
                             context);
}

static char const *
sixel_loader_libtiff_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libtiff";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libtiff_vtbl = {
    sixel_loader_libtiff_ref,
    sixel_loader_libtiff_unref,
    sixel_loader_libtiff_setopt,
    sixel_loader_libtiff_load,
    sixel_loader_libtiff_name
};

SIXELSTATUS
sixel_loader_libtiff_new(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
{
    sixel_loader_libtiff_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_libtiff_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_libtiff_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_libtiff(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_tiff(chunk);
}

#else  /* !HAVE_LIBTIFF */

/*
 * Provide a dummy symbol so that pedantic compilers do not flag the unit as
 * empty when libtiff support is disabled at configure time.
 */
enum { sixel_loader_libtiff_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBTIFF_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBTIFF_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libtiff_placeholder_function(void)
    SIXEL_LIBTIFF_PLACEHOLDER_UNUSED;

static void
sixel_loader_libtiff_placeholder_function(void)
{
    /*
     * Tie the placeholder enum to a symbol so MSVC does not warn about an
     * empty translation unit when libtiff is disabled.
     */
    (void)sixel_loader_libtiff_placeholder;
}

#undef SIXEL_LIBTIFF_PLACEHOLDER_UNUSED

#endif  /* HAVE_LIBTIFF */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
