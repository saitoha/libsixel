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
 * Lookup dispatcher that selects either the 8bit or float32 backend.
 * Backend implementations live in lookup-8bit.c and lookup-float32.c so
 * additional search structures can be added without touching callers.
 */
/*
 * IDL usage in this unit
 *
 * ILookup8Bit.configure(policy, ...);
 * ILookup8Bit.map_pixel(pixel);
 * ILookupFloat32.configure(policy, ...);
 * ILookupFloat32.map_pixel(pixel);
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>

#include <sixel.h>

#include "compat_stub.h"
#include "allocator.h"
#include "lookup-8bit.h"
#include "lookup-float32.h"
#include "lookup-common.h"

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
    sixel_allocator_t *allocator;
    sixel_lookup_8bit_t *lookup_8bit;
    sixel_lookup_float32_t *lookup_float32;
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

SIXELAPI int
sixel_lut_uses_float(sixel_lut_t const *lut)
{
    if (lut == NULL) {
        return 0;
    }

    return lut->input_is_float;
}

SIXELAPI struct sixel_lookup_8bit *
sixel_lut_backend_8bit(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return NULL;
    }

    return lut->lookup_8bit;
}

SIXELAPI struct sixel_lookup_float32 *
sixel_lut_backend_float32(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return NULL;
    }

    return lut->lookup_float32;
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
    lut->allocator = allocator;
    lut->lookup_8bit = (sixel_lookup_8bit_t *)
        malloc(sizeof(sixel_lookup_8bit_t));
    lut->lookup_float32 = (sixel_lookup_float32_t *)
        malloc(sizeof(sixel_lookup_float32_t));
    if (lut->lookup_8bit == NULL || lut->lookup_float32 == NULL) {
        free(lut->lookup_8bit);
        free(lut->lookup_float32);
        free(lut);
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_lookup_8bit_init(lut->lookup_8bit, allocator);
    if (lut->lookup_8bit->cert == NULL) {
        /*
         * CERT LUT requires its own workspace.  If allocation failed,
         * bail out early to avoid later null dereferences during
         * configuration.
         */
        sixel_lookup_8bit_finalize(lut->lookup_8bit);
        free(lut->lookup_8bit);
        free(lut->lookup_float32);
        free(lut);
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_lookup_float32_init(lut->lookup_float32, allocator);

    *out = lut;

    /* policy is normalized inside backend configure functions */
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
    int palette_depth;
    int float_components;

    palette_depth = depth;
    float_components = 0;
    /*
     * Complexion is kept in public/internal APIs for compatibility, but
     * lookup now treats it as a deprecated no-op.
     */
    complexion = 1;

    if (lut == NULL || palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Prefer float palettes when the pipeline is float32 so the LUT is built
     * in the working color space (e.g. OKLab/CIELab). This keeps lookups
     * consistent with FHEDT behavior and avoids reinterpreting RGB888 bytes as
     * float32 components.
     */
    lut->input_is_float = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat);
    if (lut->input_is_float) {
        if (palette_float != NULL && float_depth > 0) {
            if (float_depth % (int)sizeof(float) != 0) {
                sixel_helper_set_additional_message(
                    "sixel_lut_configure: float depth is invalid.");
                return SIXEL_BAD_ARGUMENT;
            }
            float_components = float_depth / (int)sizeof(float);
            if (float_components <= 0) {
                sixel_helper_set_additional_message(
                    "sixel_lut_configure: float depth has no components.");
                return SIXEL_BAD_ARGUMENT;
            }
            palette_depth = float_components;
        }

        return sixel_lookup_float32_configure(lut->lookup_float32,
                                              palette,
                                              palette_float,
                                              palette_depth,
                                              float_depth,
                                              ncolors,
                                              complexion,
                                              wcomp1,
                                              wcomp2,
                                              wcomp3,
                                              policy,
                                              pixelformat);
    }

    return sixel_lookup_8bit_configure(lut->lookup_8bit,
                                       palette,
                                       depth,
                                       ncolors,
                                       complexion,
                                       wcomp1,
                                       wcomp2,
                                       wcomp3,
                                       policy,
                                       pixelformat);
}

SIXELAPI int
sixel_lut_map_pixel(sixel_lut_t *lut, unsigned char const *pixel)
{
    if (lut == NULL) {
        return 0;
    }

    if (lut->input_is_float) {
        return sixel_lookup_float32_map_pixel(lut->lookup_float32, pixel);
    }

    return sixel_lookup_8bit_map_pixel(lut->lookup_8bit, pixel);
}

void
sixel_lut_clear(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_8bit_clear(lut->lookup_8bit);
    sixel_lookup_float32_clear(lut->lookup_float32);
    lut->input_is_float = 0;
}

SIXELAPI void
sixel_lut_unref(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lookup_8bit_finalize(lut->lookup_8bit);
    sixel_lookup_float32_finalize(lut->lookup_float32);
    free(lut->lookup_8bit);
    free(lut->lookup_float32);
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
