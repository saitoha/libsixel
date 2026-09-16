/* SPDX-License-Identifier: MIT */
/* Public loader values use an offline ICC/TIFF-byte oracle. Native-mode
 * witnesses instead compare an original-profile low-level transform on the
 * current OS; no hardcoded native toe is considered an ideal curve. */
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/loader.h"
#include "tests/data/color-numeric/tiff-managed.h"
#include "tiff_managed_test_common.h"

int
tiff_managed_check(sixel_frame_t const *frame, int fixture, int engine,
                    int raw)
{
    float const *pixels;
    double const *expected;
    double relative;
    double magnitude;
    double allowance;
    uint32_t word;
    size_t n;
    size_t i;
    size_t c;

    n = (size_t)tm_cases[fixture].width * tm_cases[fixture].height;
    if (frame->width != tm_cases[fixture].width ||
        frame->height != tm_cases[fixture].height ||
        frame->pixelformat != (raw ? SIXEL_PIXELFORMAT_RGBFLOAT32
                                  : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (raw ? SIXEL_COLORSPACE_GAMMA
                                 : SIXEL_COLORSPACE_LINEAR) ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL || frame->transparent_mask_size ||
        frame->alpha_zero_is_transparent) { return 0; }
    pixels = (float const *)frame->pixels.u8ptr;
    expected = tm_cases[fixture].expected[engine == 2 ? 0 : 1];
    relative = engine == 4 ? tm_native_relative : tm_float_relative;
    for (i = 0u; i < n; ++i) {
        magnitude = fmax(fabs(expected[i * 3u]),
            fmax(fabs(expected[i * 3u + 1u]), fabs(expected[i * 3u + 2u])));
        /* Table quantization has its own derived budget. Analytic dark
         * samples have no absolute floor; a black triplet is exact zero. */
        allowance = magnitude * relative;
        if (fixture == 5 && engine == 3 && magnitude != 0.0) {
            allowance += tm_table_absolute;
        }
        for (c = 0u; c < 3u; ++c) {
            memcpy(&word, pixels + i * 3u + c, sizeof(word));
            if (raw ? word != tm_cases[fixture].source[i * 3u + c] :
                !isfinite(pixels[i * 3u + c]) ||
                fabs(pixels[i * 3u + c] - expected[i * 3u + c]) > allowance) {
                fprintf(stderr, "TMVALUE|pixel=%zu|channel=%zu|word=%08x|"
                    "expected=%.17g|allowance=%.17g\n", i, c,
                    (unsigned int)word, expected[i * 3u + c], allowance);
                return 0;
            }
        }
    }
    return 1;
}

#if HAVE_LIBTIFF
typedef struct tm_context {
    int fixture;
    int engine;
    int native;
    int calls;
    float reference[72];
} tm_context_t;

static SIXELSTATUS
tm_capture(sixel_frame_t *frame, void *context)
{
    tm_context_t *ctx;
    sixel_frame_t wrong;
    sixel_frame_t reference;
    size_t bytes;
    int ok;

    ctx = context;
    ++ctx->calls;
    if (ctx->native == 1) {
        /* The native comparison only replaces values, never metadata. */
        reference = *frame;
        reference.pixels.u8ptr = (unsigned char *)tm_cases[ctx->fixture].source;
        reference.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        reference.colorspace = SIXEL_COLORSPACE_GAMMA;
        ok = frame->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
             frame->colorspace == SIXEL_COLORSPACE_LINEAR &&
             tiff_managed_check(&reference, ctx->fixture, ctx->engine, 1);
        bytes = (size_t)frame->width * frame->height * 3u * sizeof(float);
        ok = ok && memcmp(frame->pixels.u8ptr, ctx->reference, bytes) == 0;
    } else {
        ok = tiff_managed_check(frame, ctx->fixture, ctx->engine, 0);
        if (ok && ctx->native == 2) {
            wrong = *frame;
            wrong.width /= 2;
            wrong.height *= 2;
            ok = !tiff_managed_check(&wrong, ctx->fixture, ctx->engine, 0);
        }
    }
    return ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
tm_native(tm_context_t *ctx)
{
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    float packed[24];
    size_t i;
    int ok;

    for (i = 0u; i < 24u; ++i) {
        memcpy(packed + i, tm_cases[ctx->fixture].source + i * 3u,
               sizeof(float));
    }
    source = sixel_cms_open_profile_from_mem(tm_gray_profile,
                                              sizeof(tm_gray_profile));
    destination = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source,
        SIXEL_CMS_PIXELFORMAT_GRAY_F32, destination,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    ok = transform != NULL &&
        sixel_cms_transform_engine(transform) == SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_do_transform(transform, packed, ctx->reference, 24u);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    return ok;
}
#endif

int
tiff_managed_values(int argc, char **argv, int fixture, int engine,
                     int native)
{
#if HAVE_LIBTIFF
    sixel_loader_t *loader;
    sixel_cms_engine_t previous;
    tm_context_t context;
    SIXELSTATUS status;
    char order[128];
    int ok;

    if (argc != 2) { return 2; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    memset(&context, 0, sizeof(context));
    context.fixture = fixture;
    context.engine = engine;
    context.native = native;
    ok = native != 1 || tm_native(&context);
    fprintf(stderr, "TMVALUE|reference=%d|case-begin\n", ok);
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=linear:prefer_8bit=0!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
    loader = NULL;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                     &context);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     order);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_load_file(loader, argv[1], tm_capture);
    }
    ok = ok && status == SIXEL_OK && context.calls == 1;
    fprintf(stderr, "TMVALUE|status=%x|callbacks=%d|ok=%d\n",
            status, context.calls, ok);
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
#else
    (void)argc; (void)argv; (void)fixture; (void)engine; (void)native;
    return 77;
#endif
}
