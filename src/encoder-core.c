/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
# include <stdio.h>
# include <stdlib.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */

#if 0
#if HAVE_ASSERT_H
# include <assert.h>
#endif  /* HAVE_ASSERT_H */
#endif

#include <sixel.h>
#include "output.h"
#include "encoder-core-private.h"
#include "timer.h"

#define SIXEL_ENCODER_CORE_USEC_PER_SECOND 1000000ULL
#define SIXEL_ENCODER_CORE_USEC_PER_CENTISECOND 10000ULL

static void
sixel_encoder_core_init_defaults(sixel_output_t *output,
                                 sixel_allocator_t *allocator);
static sixel_output_t *
sixel_encoder_core_from_interface(sixel_encoder_core_t *output);
static void
sixel_encoder_core_vtbl_ref(sixel_encoder_core_t *output);
static void
sixel_encoder_core_vtbl_unref(sixel_encoder_core_t *output);
static SIXELSTATUS
sixel_encoder_core_vtbl_set_options(
    sixel_encoder_core_t *output,
    sixel_encoder_core_options_t const *options);
static SIXELSTATUS
sixel_encoder_core_vtbl_get_options(sixel_encoder_core_t *output,
                               sixel_encoder_core_options_t *options);
static SIXELSTATUS
sixel_encoder_core_vtbl_encode(
    sixel_encoder_core_t *output,
    sixel_encoder_core_encode_request_t const *request);

static sixel_encoder_core_vtbl_t const g_sixel_encoder_core_vtbl = {
    sixel_encoder_core_vtbl_ref,
    sixel_encoder_core_vtbl_unref,
    sixel_encoder_core_vtbl_set_options,
    sixel_encoder_core_vtbl_get_options,
    sixel_encoder_core_vtbl_encode
};

static long long
sixel_encoder_core_monotonic_now_usec(void)
{
    double seconds;
    double usec;

    seconds = sixel_timer_now();
    if (seconds <= 0.0) {
        return (-1);
    }
    if (seconds >=
            ((double)LLONG_MAX /
             (double)SIXEL_ENCODER_CORE_USEC_PER_SECOND)) {
        return (-1);
    }

    usec = seconds * (double)SIXEL_ENCODER_CORE_USEC_PER_SECOND;
    if (usec >= (double)LLONG_MAX) {
        return (-1);
    }
    return (long long)usec;
}

/*
 * Normalize boolean-like flags to a single byte to avoid narrowing warnings
 * when storing them in the encoder core context.
 */
static unsigned char
sixel_encoder_core_flag_to_byte(int value)
{
    unsigned char flag;

    flag = (unsigned char)(value != 0 ? 1 : 0);

    return flag;
}

SIXEL_INTERNAL_API sixel_encoder_core_t *
sixel_output_as_encoder_core(sixel_output_t const *output)
{
    return (sixel_encoder_core_t *)output;
}

static void
sixel_encoder_core_init_defaults(sixel_output_t *output,
                                 sixel_allocator_t *allocator)
{
    output->encoder_core_interface.vtbl = &g_sixel_encoder_core_vtbl;
    output->ref = 1U;
    output->writer = NULL;
    memset(&output->writer_controls, 0, sizeof(output->writer_controls));
    output->has_8bit_control = 0;
    output->has_sdm_glitch = 0;
    output->has_gri_arg_limit = 1;
    output->has_sixel_scrolling = 0;
    output->skip_dcs_envelope = 0;
    output->skip_header = 0;
    output->palette_type = SIXEL_PALETTETYPE_AUTO;
    output->colorspace = SIXEL_COLORSPACE_GAMMA;
    output->source_colorspace = SIXEL_COLORSPACE_GAMMA;
    output->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    output->save_pixel = 0;
    output->save_count = 0;
    output->active_palette = (-1);
    output->node_top = NULL;
    output->node_free = NULL;
    output->pos = 0;
    output->penetrate_multiplexer = 0;
    output->encode_policy = SIXEL_ENCODEPOLICY_AUTO;
    output->ormode = 0;
    output->transparent_policy = SIXEL_TRANSPARENT_POLICY_BACKGROUND;
    output->transparent_offset_left = 0;
    output->transparent_offset_top = 0;
    output->last_frame_time_usec = 0;
    output->allocator = allocator;
}

static sixel_output_t *
sixel_encoder_core_from_interface(sixel_encoder_core_t *output)
{
    return (sixel_output_t *)(void *)output;
}

static void
sixel_encoder_core_vtbl_ref(sixel_encoder_core_t *output)
{
    sixel_output_t *storage;

    if (output == NULL) {
        return;
    }

    storage = sixel_encoder_core_from_interface(output);
    (void)sixel_atomic_fetch_add_u32(&storage->ref, 1U);
}

static void
sixel_encoder_core_vtbl_unref(sixel_encoder_core_t *output)
{
    sixel_output_t *storage;
    unsigned int previous;

    if (output == NULL) {
        return;
    }

    storage = sixel_encoder_core_from_interface(output);
    previous = sixel_atomic_fetch_sub_u32(&storage->ref, 1U);
    if (previous == 1U) {
        sixel_encoder_core_destroy(storage);
    }
}

static SIXELSTATUS
sixel_encoder_core_vtbl_set_options(
    sixel_encoder_core_t *output,
    sixel_encoder_core_options_t const *options)
{
    sixel_output_t *storage;

    if (output == NULL || options == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_encoder_core_from_interface(output);
    storage->has_gri_arg_limit =
        sixel_encoder_core_flag_to_byte(options->has_gri_arg_limit);
    storage->palette_type = (unsigned char)options->palette_type;
    storage->encode_policy = options->encode_policy;
    storage->ormode = options->ormode;
    storage->transparent_policy = options->transparent_policy;
    storage->transparent_offset_left = options->transparent_offset_left;
    storage->transparent_offset_top = options->transparent_offset_top;
    storage->pixelformat = options->pixelformat;
    storage->source_colorspace = options->source_colorspace;
    storage->colorspace = options->colorspace;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_core_vtbl_get_options(sixel_encoder_core_t *output,
                               sixel_encoder_core_options_t *options)
{
    sixel_output_t *storage;

    if (output == NULL || options == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_encoder_core_from_interface(output);
    options->has_gri_arg_limit = storage->has_gri_arg_limit;
    options->palette_type = storage->palette_type;
    options->encode_policy = storage->encode_policy;
    options->ormode = storage->ormode;
    options->transparent_policy = storage->transparent_policy;
    options->transparent_offset_left = storage->transparent_offset_left;
    options->transparent_offset_top = storage->transparent_offset_top;
    options->pixelformat = storage->pixelformat;
    options->source_colorspace = storage->source_colorspace;
    options->colorspace = storage->colorspace;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_encoder_core_vtbl_encode(
    sixel_encoder_core_t *output,
    sixel_encoder_core_encode_request_t const *request)
{
    if (output == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_encoder_core_encode_dispatch(request);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_get_encoder_options(sixel_output_t *output,
                                 sixel_encoder_core_options_t *options)
{
    if (output == NULL || options == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    options->has_gri_arg_limit = output->has_gri_arg_limit;
    options->palette_type = output->palette_type;
    options->encode_policy = output->encode_policy;
    options->ormode = output->ormode;
    options->transparent_policy = output->transparent_policy;
    options->transparent_offset_left = output->transparent_offset_left;
    options->transparent_offset_top = output->transparent_offset_top;
    options->pixelformat = output->pixelformat;
    options->source_colorspace = output->source_colorspace;
    options->colorspace = output->colorspace;

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_set_encoder_options(
    sixel_output_t *output,
    sixel_encoder_core_options_t const *options)
{
    if (output == NULL || options == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    output->has_gri_arg_limit =
        sixel_encoder_core_flag_to_byte(options->has_gri_arg_limit);
    output->palette_type = (unsigned char)options->palette_type;
    output->encode_policy = options->encode_policy;
    output->ormode = options->ormode;
    output->transparent_policy = options->transparent_policy;
    output->transparent_offset_left = options->transparent_offset_left;
    output->transparent_offset_top = options->transparent_offset_top;
    output->pixelformat = options->pixelformat;
    output->source_colorspace = options->source_colorspace;
    output->colorspace = options->colorspace;

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_get_writer_controls(sixel_output_t *output,
                                 sixel_writer_controls_t *controls)
{
    if (output == NULL || controls == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *controls = output->writer_controls;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_set_writer_controls(
    sixel_output_t *output,
    sixel_writer_controls_t const *controls)
{
    SIXELSTATUS status;

    if (output == NULL || controls == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    output->writer_controls.has_8bit_control =
        sixel_encoder_core_flag_to_byte(controls->has_8bit_control);
    output->writer_controls.has_sixel_scrolling =
        sixel_encoder_core_flag_to_byte(controls->has_sixel_scrolling);
    output->writer_controls.has_sdm_glitch =
        sixel_encoder_core_flag_to_byte(controls->has_sdm_glitch);
    output->writer_controls.skip_dcs_envelope =
        sixel_encoder_core_flag_to_byte(controls->skip_dcs_envelope);
    output->writer_controls.skip_header =
        sixel_encoder_core_flag_to_byte(controls->skip_header);
    output->writer_controls.penetrate_multiplexer =
        sixel_encoder_core_flag_to_byte(controls->penetrate_multiplexer);

    output->has_8bit_control =
        (unsigned char)output->writer_controls.has_8bit_control;
    output->has_sixel_scrolling =
        (unsigned char)output->writer_controls.has_sixel_scrolling;
    output->has_sdm_glitch =
        (unsigned char)output->writer_controls.has_sdm_glitch;
    output->skip_dcs_envelope =
        (unsigned char)output->writer_controls.skip_dcs_envelope;
    output->skip_header = (unsigned char)output->writer_controls.skip_header;
    output->penetrate_multiplexer =
        output->writer_controls.penetrate_multiplexer;

    if (output->writer == NULL || output->writer->vtbl == NULL ||
        output->writer->vtbl->set_controls == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = output->writer->vtbl->set_controls(output->writer,
                                                &output->writer_controls);
    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_init_writer(sixel_output_t *output,
                         sixel_write_function fn_write,
                         void *priv)
{
    sixel_writer_init_request_t request;

    if (output == NULL || output->writer == NULL ||
        output->writer->vtbl == NULL ||
        output->writer->vtbl->init == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    request.fn_write = fn_write;
    request.priv = priv;

    return output->writer->vtbl->init(output->writer, &request);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_set_encoder_format(sixel_output_t *output,
                                int pixelformat,
                                int source_colorspace,
                                int colorspace)
{
    if (output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    output->pixelformat = pixelformat;
    output->source_colorspace = source_colorspace;
    output->colorspace = colorspace;

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_compute_frame_delay(sixel_output_t *output,
                                 int delay_cs,
                                 sixel_output_frame_delay_t *delay)
{
    sixel_output_t *storage;
    unsigned long long target_usec64;
    unsigned long long remaining_usec64;
    long long now_usec;
    long long elapsed_usec64;

    if (output == NULL || delay == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    delay->elapsed_usec = 0;
    delay->target_usec = 0U;
    delay->remaining_usec = 0U;
    if (delay_cs <= 0) {
        return SIXEL_OK;
    }

    storage = output;
    target_usec64 = (unsigned long long)delay_cs *
        SIXEL_ENCODER_CORE_USEC_PER_CENTISECOND;
    if (target_usec64 > (unsigned long long)UINT_MAX) {
        target_usec64 = (unsigned long long)UINT_MAX;
    }
    delay->target_usec = (unsigned int)target_usec64;

    now_usec = sixel_encoder_core_monotonic_now_usec();
    if (now_usec < 0) {
        storage->last_frame_time_usec = 0;
        delay->remaining_usec = (unsigned int)target_usec64;
        return SIXEL_OK;
    }

    if (storage->last_frame_time_usec <= 0) {
        elapsed_usec64 = 0;
    } else {
        elapsed_usec64 = now_usec - storage->last_frame_time_usec;
        if (elapsed_usec64 < 0) {
            elapsed_usec64 = 0;
        }
    }
    storage->last_frame_time_usec = now_usec;

    if (elapsed_usec64 > INT_MAX) {
        delay->elapsed_usec = INT_MAX;
    } else {
        delay->elapsed_usec = (int)elapsed_usec64;
    }

    if ((unsigned long long)elapsed_usec64 >= target_usec64) {
        return SIXEL_OK;
    }

    remaining_usec64 = target_usec64 - (unsigned long long)elapsed_usec64;
    if (remaining_usec64 > target_usec64) {
        remaining_usec64 = target_usec64;
    }
    if (remaining_usec64 > (unsigned long long)UINT_MAX) {
        remaining_usec64 = (unsigned long long)UINT_MAX;
    }
    delay->remaining_usec = (unsigned int)remaining_usec64;

    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_begin_image(sixel_output_t *output,
                         int width,
                         int height,
                         int parameter0,
                         int parameter1,
                         int parameter2,
                         int parameter_count,
                         int use_raster_attributes)
{
    sixel_writer_image_header_t header;

    if (output == NULL || output->writer == NULL ||
        output->writer->vtbl == NULL ||
        output->writer->vtbl->begin_image == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    header.parameter0 = parameter0;
    header.parameter1 = parameter1;
    header.parameter2 = parameter2;
    header.parameter_count = parameter_count;
    header.width = width;
    header.height = height;
    header.use_raster_attributes = use_raster_attributes;

    return output->writer->vtbl->begin_image(output->writer, &header);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_write_bytes(sixel_output_t *output,
                         char const *data,
                         int size)
{
    if (output == NULL || output->writer == NULL ||
        output->writer->vtbl == NULL ||
        output->writer->vtbl->write == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return output->writer->vtbl->write(output->writer, data, size);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_end_image(sixel_output_t *output)
{
    if (output == NULL || output->writer == NULL ||
        output->writer->vtbl == NULL ||
        output->writer->vtbl->end_image == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return output->writer->vtbl->end_image(output->writer);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_output_t *output;
    SIXELSTATUS status;
    size_t size;

    output = NULL;
    status = SIXEL_FALSE;
    size = 0u;
    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *object = NULL;
    size = sizeof(sixel_output_t) + SIXEL_OUTPUT_PACKET_SIZE * 2;
    output = (sixel_output_t *)sixel_allocator_malloc(allocator, size);
    if (output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_core_factory_new: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_encoder_core_init_defaults(output, allocator);
    sixel_allocator_ref(allocator);
    status = sixel_writer_factory_new(allocator, (void **)&output->writer);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, output);
        sixel_allocator_unref(allocator);
        return status;
    }
    *object = sixel_output_as_encoder_core(output);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_normal_factory_new(sixel_allocator_t *allocator,
                                      void **object)
{
    return sixel_encoder_core_factory_new(allocator, object);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_highcolor_factory_new(sixel_allocator_t *allocator,
                                         void **object)
{
    return sixel_encoder_core_factory_new(allocator, object);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_encoder_core_ormode_factory_new(sixel_allocator_t *allocator,
                                      void **object)
{
    return sixel_encoder_core_factory_new(allocator, object);
}

SIXEL_INTERNAL_API void
sixel_encoder_core_destroy(sixel_output_t *output)
{
    sixel_allocator_t *allocator;

    if (output) {
        allocator = output->allocator;
        if (output->writer != NULL && output->writer->vtbl != NULL &&
            output->writer->vtbl->unref != NULL) {
            output->writer->vtbl->unref(output->writer);
        }
        sixel_allocator_free(allocator, output);
        sixel_allocator_unref(allocator);
    }
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
