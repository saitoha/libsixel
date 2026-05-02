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
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */

#if 0
#if HAVE_ASSERT_H
# include <assert.h>
#endif  /* HAVE_ASSERT_H */
#endif

#include <sixel.h>
#include "output.h"
#include "sixel_atomic.h"

static void
sixel_output_init_defaults(sixel_output_t *output,
                           sixel_write_function fn_write,
                           void *priv,
                           sixel_allocator_t *allocator);
static sixel_output_t *
sixel_output_from_interface(sixel_output_interface_t *output);
static void
sixel_output_vtbl_ref(sixel_output_interface_t *output);
static void
sixel_output_vtbl_unref(sixel_output_interface_t *output);
static SIXELSTATUS
sixel_output_vtbl_init_writer(
    sixel_output_interface_t *output,
    sixel_output_writer_request_t const *request);
static SIXELSTATUS
sixel_output_vtbl_set_options(
    sixel_output_interface_t *output,
    sixel_output_options_t const *options);
static SIXELSTATUS
sixel_output_vtbl_get_options(sixel_output_interface_t *output,
                              sixel_output_options_t *options);
static SIXELSTATUS
sixel_output_vtbl_set_format(
    sixel_output_interface_t *output,
    sixel_output_format_t const *format);
static SIXELSTATUS
sixel_output_vtbl_write(sixel_output_interface_t *output,
                        char const *data,
                        int size);
static sixel_allocator_t *
sixel_output_vtbl_allocator(sixel_output_interface_t *output);

static sixel_output_vtbl_t const g_sixel_output_vtbl = {
    sixel_output_vtbl_ref,
    sixel_output_vtbl_unref,
    sixel_output_vtbl_init_writer,
    sixel_output_vtbl_set_options,
    sixel_output_vtbl_get_options,
    sixel_output_vtbl_set_format,
    sixel_output_vtbl_write,
    sixel_output_vtbl_allocator
};

/*
 * Normalize boolean-like flags to a single byte to avoid narrowing warnings
 * when storing them in the output context.
 */
static unsigned char
sixel_output_flag_to_byte(int value)
{
    unsigned char flag;

    flag = (unsigned char)(value != 0 ? 1 : 0);

    return flag;
}

static void
sixel_output_init_defaults(sixel_output_t *output,
                           sixel_write_function fn_write,
                           void *priv,
                           sixel_allocator_t *allocator)
{
    output->output_interface.vtbl = &g_sixel_output_vtbl;
    output->ref = 1U;
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
    output->fn_write = fn_write;
    output->save_pixel = 0;
    output->save_count = 0;
    output->active_palette = (-1);
    output->node_top = NULL;
    output->node_free = NULL;
    output->priv = priv;
    output->pos = 0;
    output->penetrate_multiplexer = 0;
    output->encode_policy = SIXEL_ENCODEPOLICY_AUTO;
    output->ormode = 0;
    output->last_frame_time_usec = 0;
    output->allocator = allocator;
}

static sixel_output_t *
sixel_output_from_interface(sixel_output_interface_t *output)
{
    return (sixel_output_t *)(void *)output;
}

static void
sixel_output_vtbl_ref(sixel_output_interface_t *output)
{
    sixel_output_t *storage;

    if (output == NULL) {
        return;
    }

    storage = sixel_output_from_interface(output);
    (void)sixel_atomic_fetch_add_u32(&storage->ref, 1U);
}

static void
sixel_output_vtbl_unref(sixel_output_interface_t *output)
{
    sixel_output_t *storage;
    unsigned int previous;

    if (output == NULL) {
        return;
    }

    storage = sixel_output_from_interface(output);
    previous = sixel_atomic_fetch_sub_u32(&storage->ref, 1U);
    if (previous == 1U) {
        sixel_output_destroy(storage);
    }
}

static SIXELSTATUS
sixel_output_vtbl_init_writer(
    sixel_output_interface_t *output,
    sixel_output_writer_request_t const *request)
{
    sixel_output_t *storage;

    if (output == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_output_from_interface(output);
    storage->fn_write = request->fn_write;
    storage->priv = request->priv;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_output_vtbl_set_options(
    sixel_output_interface_t *output,
    sixel_output_options_t const *options)
{
    sixel_output_t *storage;

    if (output == NULL || options == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_output_from_interface(output);
    storage->has_8bit_control =
        sixel_output_flag_to_byte(options->has_8bit_control);
    storage->has_sixel_scrolling =
        sixel_output_flag_to_byte(options->has_sixel_scrolling);
    storage->has_gri_arg_limit =
        sixel_output_flag_to_byte(options->has_gri_arg_limit);
    storage->has_sdm_glitch =
        sixel_output_flag_to_byte(options->has_sdm_glitch);
    storage->skip_dcs_envelope =
        sixel_output_flag_to_byte(options->skip_dcs_envelope);
    storage->skip_header = sixel_output_flag_to_byte(options->skip_header);
    storage->palette_type = (unsigned char)options->palette_type;
    storage->penetrate_multiplexer = options->penetrate_multiplexer;
    storage->encode_policy = options->encode_policy;
    storage->ormode = options->ormode;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_output_vtbl_get_options(sixel_output_interface_t *output,
                              sixel_output_options_t *options)
{
    sixel_output_t *storage;

    if (output == NULL || options == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_output_from_interface(output);
    options->has_8bit_control = storage->has_8bit_control;
    options->has_sixel_scrolling = storage->has_sixel_scrolling;
    options->has_gri_arg_limit = storage->has_gri_arg_limit;
    options->has_sdm_glitch = storage->has_sdm_glitch;
    options->skip_dcs_envelope = storage->skip_dcs_envelope;
    options->skip_header = storage->skip_header;
    options->palette_type = storage->palette_type;
    options->penetrate_multiplexer = storage->penetrate_multiplexer;
    options->encode_policy = storage->encode_policy;
    options->ormode = storage->ormode;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_output_vtbl_set_format(
    sixel_output_interface_t *output,
    sixel_output_format_t const *format)
{
    sixel_output_t *storage;

    if (output == NULL || format == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_output_from_interface(output);
    storage->pixelformat = format->pixelformat;
    storage->source_colorspace = format->source_colorspace;
    storage->colorspace = format->colorspace;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_output_vtbl_write(sixel_output_interface_t *output,
                        char const *data,
                        int size)
{
    sixel_output_t *storage;
    int result;

    if (output == NULL || data == NULL || size < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (size == 0) {
        return SIXEL_OK;
    }

    storage = sixel_output_from_interface(output);
    if (storage->fn_write == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result = storage->fn_write((char *)data, size, storage->priv);
    if (result < 0) {
#if HAVE_ERRNO_H
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
#else
        return SIXEL_LIBC_ERROR;
#endif
    }
    return SIXEL_OK;
}

static sixel_allocator_t *
sixel_output_vtbl_allocator(sixel_output_interface_t *output)
{
    sixel_output_t *storage;

    if (output == NULL) {
        return NULL;
    }

    storage = sixel_output_from_interface(output);
    return storage->allocator;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_output_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_output_t *output;
    size_t size;

    output = NULL;
    size = 0u;
    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *object = NULL;
    size = sizeof(sixel_output_t) + SIXEL_OUTPUT_PACKET_SIZE * 2;
    output = (sixel_output_t *)sixel_allocator_malloc(allocator, size);
    if (output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_output_factory_new: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_output_init_defaults(output, NULL, NULL, allocator);
    sixel_allocator_ref(allocator);
    *object = sixel_output_as_interface(output);
    return SIXEL_OK;
}


/* create new output context object */
SIXELAPI SIXELSTATUS
sixel_output_new(
    sixel_output_t          /* out */ **output,
    sixel_write_function    /* in */  fn_write,
    void                    /* in */  *priv,
    sixel_allocator_t       /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *local_allocator;
    sixel_output_interface_t *interface;
    sixel_output_writer_request_t request;
    void *object;
    int allocator_owned;

    if (output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *output = NULL;
    local_allocator = allocator;
    interface = NULL;
    object = NULL;
    allocator_owned = 0;
    if (local_allocator == NULL) {
        status = sixel_allocator_new(&local_allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        allocator_owned = 1;
    }

    status = sixel_output_factory_new(local_allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    interface = (sixel_output_interface_t *)object;
    request.fn_write = fn_write;
    request.priv = priv;
    status = interface->vtbl->init_writer(interface, &request);
    if (SIXEL_FAILED(status)) {
        interface->vtbl->unref(interface);
        goto end;
    }

    *output = (sixel_output_t *)object;
    object = NULL;
    status = SIXEL_OK;

end:
    if (allocator_owned && local_allocator != NULL) {
        sixel_allocator_unref(local_allocator);
    }
    return status;
}


/* deprecated: create an output object */
SIXELAPI sixel_output_t *
sixel_output_create(sixel_write_function fn_write, void *priv)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_output_t *output = NULL;

    status = sixel_output_new(&output, fn_write, priv, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return output;
}


/* destroy output context object */
SIXELAPI void
sixel_output_destroy(sixel_output_t *output)
{
    sixel_allocator_t *allocator;

    if (output) {
        allocator = output->allocator;
        sixel_allocator_free(allocator, output);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of output context object (thread-safe) */
SIXELAPI void
sixel_output_ref(sixel_output_t *output)
{
    sixel_output_interface_t *interface;

    if (output == NULL) {
        return;
    }

    interface = sixel_output_as_interface(output);
    if (interface != NULL && interface->vtbl != NULL &&
        interface->vtbl->ref != NULL) {
        interface->vtbl->ref(interface);
    }
}


/* decrease reference count of output context object (thread-safe) */
SIXELAPI void
sixel_output_unref(sixel_output_t *output)
{
    sixel_output_interface_t *interface;

    if (output == NULL) {
        return;
    }

    interface = sixel_output_as_interface(output);
    if (interface != NULL && interface->vtbl != NULL &&
        interface->vtbl->unref != NULL) {
        interface->vtbl->unref(interface);
    }
}


/* get 8bit output mode which indicates whether it uses C1 control characters */
SIXELAPI int
sixel_output_get_8bit_availability(sixel_output_t *output)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return 0;
    }

    return options.has_8bit_control;
}


/* set 8bit output mode state */
SIXELAPI void
sixel_output_set_8bit_availability(sixel_output_t *output, int availability)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.has_8bit_control = availability;
    (void)interface->vtbl->set_options(interface, &options);
}


/* set whether limit arguments of DECGRI('!') to 255 */
SIXELAPI void
sixel_output_set_gri_arg_limit(
    sixel_output_t /* in */ *output, /* output context */
    int            /* in */ value)   /* 0: don't limit arguments of DECGRI
                                        1: limit arguments of DECGRI to 255 */
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.has_gri_arg_limit = value;
    (void)interface->vtbl->set_options(interface, &options);
}


/* set GNU Screen penetration feature enable or disable */
SIXELAPI void
sixel_output_set_penetrate_multiplexer(sixel_output_t *output, int penetrate)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.penetrate_multiplexer = penetrate;
    (void)interface->vtbl->set_options(interface, &options);
}


/* set whether we skip DCS envelope */
SIXELAPI void
sixel_output_set_skip_dcs_envelope(sixel_output_t *output, int skip)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.skip_dcs_envelope = skip;
    (void)interface->vtbl->set_options(interface, &options);
}


SIXELAPI void
sixel_output_set_skip_header(sixel_output_t *output, int skip)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.skip_header = skip;
    (void)interface->vtbl->set_options(interface, &options);
}


/* set palette type: RGB or HLS */
SIXELAPI void
sixel_output_set_palette_type(sixel_output_t *output, int palettetype)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.palette_type = palettetype;
    (void)interface->vtbl->set_options(interface, &options);
}


SIXELAPI void
sixel_output_set_ormode(sixel_output_t *output, int ormode)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.ormode = ormode;
    (void)interface->vtbl->set_options(interface, &options);
}


/* set encodeing policy: auto, fast or size */
SIXELAPI void
sixel_output_set_encode_policy(sixel_output_t *output, int encode_policy)
{
    sixel_output_interface_t *interface;
    sixel_output_options_t options;

    interface = sixel_output_as_interface(output);
    if (interface == NULL || interface->vtbl == NULL ||
        interface->vtbl->get_options == NULL ||
        interface->vtbl->set_options == NULL ||
        SIXEL_FAILED(interface->vtbl->get_options(interface, &options))) {
        return;
    }
    options.encode_policy = encode_policy;
    (void)interface->vtbl->set_options(interface, &options);
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
