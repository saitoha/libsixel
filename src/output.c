/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sixel.h>

#include "output.h"
#include "output-factory.h"

/* create new output context object */
SIXELAPI SIXELSTATUS
sixel_output_new(
    sixel_output_t          /* out */ **output,
    sixel_write_function    /* in */  fn_write,
    void                    /* in */  *priv,
    sixel_allocator_t       /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_allocator_t *local_allocator;
    int allocator_owned;

    status = SIXEL_FALSE;
    local_allocator = allocator;
    allocator_owned = 0;

    if (output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *output = NULL;
    if (local_allocator == NULL) {
        status = sixel_allocator_new(&local_allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        allocator_owned = 1;
    }

    status = sixel_encoder_core_create_output_from_factory(output,
                                                           fn_write,
                                                           priv,
                                                           local_allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

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
    SIXELSTATUS status;
    sixel_output_t *output;

    status = SIXEL_FALSE;
    output = NULL;

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
    sixel_encoder_core_destroy(output);
}

/* increase reference count of output context object (thread-safe) */
SIXELAPI void
sixel_output_ref(sixel_output_t *output)
{
    sixel_encoder_core_t *core;

    if (output == NULL) {
        return;
    }

    core = sixel_output_as_encoder_core(output);
    if (core != NULL && core->vtbl != NULL &&
        core->vtbl->ref != NULL) {
        core->vtbl->ref(core);
    }
}

/* decrease reference count of output context object (thread-safe) */
SIXELAPI void
sixel_output_unref(sixel_output_t *output)
{
    sixel_encoder_core_t *core;

    if (output == NULL) {
        return;
    }

    core = sixel_output_as_encoder_core(output);
    if (core != NULL && core->vtbl != NULL &&
        core->vtbl->unref != NULL) {
        core->vtbl->unref(core);
    }
}

/* get 8bit output mode which indicates whether it uses C1 control characters */
SIXELAPI int
sixel_output_get_8bit_availability(sixel_output_t *output)
{
    sixel_writer_controls_t controls;

    if (SIXEL_FAILED(sixel_output_get_writer_controls(output, &controls))) {
        return 0;
    }

    return controls.has_8bit_control;
}

/* set 8bit output mode state */
SIXELAPI void
sixel_output_set_8bit_availability(sixel_output_t *output, int availability)
{
    sixel_writer_controls_t controls;

    if (SIXEL_FAILED(sixel_output_get_writer_controls(output, &controls))) {
        return;
    }
    controls.has_8bit_control = availability;
    (void)sixel_output_set_writer_controls(output, &controls);
}

/* set whether limit arguments of DECGRI('!') to 255 */
SIXELAPI void
sixel_output_set_gri_arg_limit(
    sixel_output_t /* in */ *output, /* output context */
    int            /* in */ value)   /* 0: don't limit arguments of DECGRI
                                        1: limit arguments of DECGRI to 255 */
{
    sixel_encoder_core_options_t options;

    if (SIXEL_FAILED(sixel_output_get_encoder_options(output, &options))) {
        return;
    }
    options.has_gri_arg_limit = value;
    (void)sixel_output_set_encoder_options(output, &options);
}

/* set GNU Screen penetration feature enable or disable */
SIXELAPI void
sixel_output_set_penetrate_multiplexer(sixel_output_t *output, int penetrate)
{
    sixel_writer_controls_t controls;

    if (SIXEL_FAILED(sixel_output_get_writer_controls(output, &controls))) {
        return;
    }
    controls.penetrate_multiplexer = penetrate;
    (void)sixel_output_set_writer_controls(output, &controls);
}

/* set whether we skip DCS envelope */
SIXELAPI void
sixel_output_set_skip_dcs_envelope(sixel_output_t *output, int skip)
{
    sixel_writer_controls_t controls;

    if (SIXEL_FAILED(sixel_output_get_writer_controls(output, &controls))) {
        return;
    }
    controls.skip_dcs_envelope = skip;
    (void)sixel_output_set_writer_controls(output, &controls);
}

SIXELAPI void
sixel_output_set_skip_header(sixel_output_t *output, int skip)
{
    sixel_writer_controls_t controls;

    if (SIXEL_FAILED(sixel_output_get_writer_controls(output, &controls))) {
        return;
    }
    controls.skip_header = skip;
    (void)sixel_output_set_writer_controls(output, &controls);
}

/* set palette type: RGB or HLS */
SIXELAPI void
sixel_output_set_palette_type(sixel_output_t *output, int palettetype)
{
    sixel_encoder_core_options_t options;

    if (SIXEL_FAILED(sixel_output_get_encoder_options(output, &options))) {
        return;
    }
    options.palette_type = palettetype;
    (void)sixel_output_set_encoder_options(output, &options);
}

SIXELAPI void
sixel_output_set_ormode(sixel_output_t *output, int ormode)
{
    sixel_encoder_core_options_t options;

    if (SIXEL_FAILED(sixel_output_get_encoder_options(output, &options))) {
        return;
    }
    options.ormode = ormode;
    (void)sixel_output_set_encoder_options(output, &options);
}

/* set encoding policy: auto, fast or size */
SIXELAPI void
sixel_output_set_encode_policy(sixel_output_t *output, int encode_policy)
{
    sixel_encoder_core_options_t options;

    if (SIXEL_FAILED(sixel_output_get_encoder_options(output, &options))) {
        return;
    }
    options.encode_policy = encode_policy;
    (void)sixel_output_set_encoder_options(output, &options);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
