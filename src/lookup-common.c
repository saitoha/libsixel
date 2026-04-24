/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

/*
 * IDL usage in this unit
 *
 * interface ILut {
 *   configure(...);
 *   map_pixel(pixel);
 *   clear();
 * }
 *
 * Ownership/lifetime:
 * - sixel_lut owns one ILookupPolicy instance while configured.
 *
 * Creation path:
 * - select_name(request) -> services/factory.create(name) -> prepare(request)
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "compat_stub.h"
#include "components.h"
#include "factory.h"
#include "lookup-common.h"
#include "lookup-policy.h"

#if SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) && \
        !defined(WITH_WINPTHREAD)
#  define SIXEL_LOOKUP_USE_WIN32_ONCE 1
#  include <windows.h>
static INIT_ONCE sixel_lookup_once = INIT_ONCE_STATIC_INIT;
# else
#  include <pthread.h>
static pthread_once_t sixel_lookup_once = PTHREAD_ONCE_INIT;
# endif
#endif

struct sixel_lut {
    int input_is_float;
    int prefer_palette_float_lookup;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *policy;
};

static int sixel_lookup_parallel_active = 0;
static int sixel_lookup_certlut_shared = -1;
static int sixel_lookup_5bit_shared = -1;
static int sixel_lookup_6bit_shared = -1;

static int
sixel_lookup_parse_flag(char const *text, int default_value);

static void
sixel_lookup_init_shared_flags(void)
{
    sixel_lookup_certlut_shared = sixel_lookup_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE"),
        0);
    sixel_lookup_5bit_shared = sixel_lookup_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_5BIT_SHARED_INSTANCE"),
        1);
    sixel_lookup_6bit_shared = sixel_lookup_parse_flag(
        sixel_compat_getenv("SIXEL_LOOKUP_6BIT_SHARED_INSTANCE"),
        1);
}

#if SIXEL_ENABLE_THREADS && defined(SIXEL_LOOKUP_USE_WIN32_ONCE)
static BOOL CALLBACK
sixel_lookup_init_shared_flags_once_cb(PINIT_ONCE init_once,
                                       PVOID parameter,
                                       PVOID *context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    sixel_lookup_init_shared_flags();

    return TRUE;
}
#endif

static void
sixel_lookup_init_shared_flags_once(void)
{
#if SIXEL_ENABLE_THREADS
# if defined(SIXEL_LOOKUP_USE_WIN32_ONCE)
    BOOL executed;

    executed = InitOnceExecuteOnce(&sixel_lookup_once,
                                   sixel_lookup_init_shared_flags_once_cb,
                                   NULL,
                                   NULL);
    if (executed == FALSE) {
        sixel_lookup_init_shared_flags();
    }
# else
    int status;

    status = pthread_once(&sixel_lookup_once,
                          sixel_lookup_init_shared_flags);
    if (status != 0) {
        sixel_lookup_init_shared_flags();
    }
# endif
#else
    if (sixel_lookup_certlut_shared < 0
            || sixel_lookup_5bit_shared < 0
            || sixel_lookup_6bit_shared < 0) {
        sixel_lookup_init_shared_flags();
    }
#endif
}

static int
sixel_lookup_parse_flag(char const *text, int default_value)
{
    int value;

    value = default_value;
    if (text != NULL) {
        if (text[0] == '1' && text[1] == '\0') {
            value = 1;
        } else if (text[0] == '0' && text[1] == '\0') {
            value = 0;
        }
    }

    return value;
}

SIXELAPI int
sixel_lookup_env_shared_certlut(void)
{
    sixel_lookup_init_shared_flags_once();
    return sixel_lookup_certlut_shared;
}

SIXELAPI int
sixel_lookup_env_shared_5bit(void)
{
    sixel_lookup_init_shared_flags_once();
    return sixel_lookup_5bit_shared;
}

SIXELAPI int
sixel_lookup_env_shared_6bit(void)
{
    sixel_lookup_init_shared_flags_once();
    return sixel_lookup_6bit_shared;
}

SIXELAPI void
sixel_lookup_set_parallel_dither_active(int active)
{
    sixel_lookup_parallel_active = (active != 0) ? 1 : 0;
}

SIXELAPI int
sixel_lookup_parallel_dither_active(void)
{
    return sixel_lookup_parallel_active;
}

SIXELAPI SIXELSTATUS
sixel_lut_new(sixel_lut_t **out,
              int policy,
              sixel_allocator_t *allocator)
{
    sixel_lut_t *lut;

    if (out == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut = (sixel_lut_t *)malloc(sizeof(sixel_lut_t));
    if (lut == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    lut->input_is_float = 0;
    lut->prefer_palette_float_lookup = 0;
    lut->allocator = allocator;
    lut->policy = NULL;

    *out = lut;

    /* Policy selection happens at configure time. */
    (void)policy;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lut_configure(sixel_lut_t *lut,
                    unsigned char const *palette,
                    float const *palette_float,
                    int depth,
                    int float_depth,
                    int ncolors,
                    int complexion,
                    int wcomp1,
                    int wcomp2,
                    int wcomp3,
                    int policy,
                    int pixelformat)
{
    SIXELSTATUS status;
    sixel_lookup_policy_select_request_t select_request;
    sixel_lookup_policy_prepare_request_t prepare_request;
    char const *policy_name;
    sixel_lookup_policy_interface_t *created_policy;
    sixel_lookup_policy_interface_t *previous_policy;
    sixel_factory_t *factory;
    void *service;
    int optimize_lookup;

    status = SIXEL_FALSE;
    memset(&select_request, 0, sizeof(select_request));
    memset(&prepare_request, 0, sizeof(prepare_request));
    policy_name = NULL;
    created_policy = NULL;
    previous_policy = NULL;
    factory = NULL;
    service = NULL;
    optimize_lookup = 0;
    /*
     * Complexion is kept in public/internal APIs for compatibility, but
     * lookup now treats it as a deprecated no-op.
     */
    complexion = 1;
    (void)wcomp1;
    (void)wcomp2;
    (void)wcomp3;

    if (lut == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Prefer float palettes when the pipeline is float32 so the LUT is built
     * in the working color space (e.g. OKLab/CIELab). This keeps lookups
     * consistent with FHEDT behavior and avoids reinterpreting RGB888 bytes as
     * float32 components.
     */
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
            && palette_float != NULL
            && float_depth > 0
            && float_depth % (int)sizeof(float) != 0) {
        sixel_helper_set_additional_message(
            "sixel_lut_configure: float depth is invalid.");
        return SIXEL_BAD_ARGUMENT;
    }

    optimize_lookup = (policy != SIXEL_LUT_POLICY_NONE);
    select_request.palette = palette;
    select_request.depth = depth;
    select_request.reqcolor = ncolors;
    select_request.optimize_lookup = optimize_lookup;
    select_request.lut_policy = policy;
    policy_name = sixel_lookup_policy_select_name(&select_request);
    if (policy_name == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;

    status = factory->vtbl->create(factory,
                                   policy_name,
                                   (void **)&created_policy);
    factory->vtbl->unref(factory);
    factory = NULL;
    if (SIXEL_FAILED(status)) {
        return status;
    }

    prepare_request.palette = palette;
    prepare_request.palette_float = palette_float;
    prepare_request.depth = depth;
    prepare_request.float_depth = float_depth;
    prepare_request.reqcolor = ncolors;
    prepare_request.complexion = complexion;
    prepare_request.pixelformat = pixelformat;
    prepare_request.reuse_policy = lut->policy;
    prepare_request.reuse_policy_slot = NULL;
    prepare_request.allocator = lut->allocator;
    status = created_policy->vtbl->prepare(created_policy, &prepare_request);
    if (SIXEL_FAILED(status)) {
        created_policy->vtbl->unref(created_policy);
        return status;
    }

    previous_policy = lut->policy;
    lut->policy = created_policy;
    lut->input_is_float =
        lut->policy->vtbl->lookup_source_is_float(lut->policy);
    lut->prefer_palette_float_lookup =
        lut->policy->vtbl->prefer_palette_float_lookup(lut->policy);
    if (previous_policy != NULL) {
        previous_policy->vtbl->unref(previous_policy);
    }

    return SIXEL_OK;
}

SIXELAPI int
sixel_lut_map_pixel(sixel_lut_t *lut, unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL || lut->policy == NULL) {
        return 0;
    }

    return lut->policy->vtbl->map_pixel(lut->policy, pixel);
}

void
sixel_lut_clear(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    if (lut->policy != NULL) {
        lut->policy->vtbl->unref(lut->policy);
        lut->policy = NULL;
    }
    lut->input_is_float = 0;
    lut->prefer_palette_float_lookup = 0;
}

SIXELAPI void
sixel_lut_unref(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lut_clear(lut);
    free(lut);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
