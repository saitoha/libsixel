/* SPDX-License-Identifier: MIT */
/* Public values are literal IEEE words or offline rational 1-x results. */
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "tests/data/color-numeric/tiff-float.h"
#include "tiff_float_test_common.h"

#if HAVE_LIBTIFF
typedef struct tuf_context {
    int variant;
    int calls;
} tuf_context_t;

static int
tuf_check(sixel_frame_t const *frame, int variant)
{
    static int const photos[18] = {
        2, 2, 1, 1, 0, 0, 2, 1, 0, 2, 2, 2, 1, 0, 2, 2, 2, 2
    };
    uint32_t const *expected;
    unsigned char const *pixels;
    uint32_t actual;
    size_t i;
    size_t c;
    size_t x;
    size_t y;
    size_t source;
    size_t count;
    int width;
    int height;
    int photo;

    photo = photos[variant];
    width = photo == 2 ? 4 : 6;
    height = photo == 2 ? 2 : 4;
    if (frame->width != width || frame->height != height ||
        frame->pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_GAMMA ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL || frame->transparent_mask_size ||
        frame->alpha_zero_is_transparent) {
        return 0;
    }
    expected = photo == 0 ? tuf_white : tuf_words;
    pixels = sixel_frame_get_pixels((sixel_frame_t *)frame);
    count = (size_t)width * (size_t)height;
    for (i = 0u; i < count; ++i) {
        x = i % (size_t)width;
        y = i / (size_t)width;
        if (variant == 6 || variant == 7) { x = width - 1u - x; }
        if (variant == 6 || variant == 8) { y = height - 1u - y; }
        source = y * (size_t)width + x;
        for (c = 0u; c < 3u; ++c) {
            memcpy(&actual, pixels + (i * 3u + c) * sizeof(float),
                   sizeof(actual));
            if (actual != expected[photo == 2 ? source * 3u + c : source]) {
                fprintf(stderr, "TFLOAT|pixel=%zu|channel=%zu|word=%08x\n",
                        i, c, (unsigned int)actual);
                return 0;
            }
        }
    }
    return 1;
}

static SIXELSTATUS
tuf_capture(sixel_frame_t *frame, void *context)
{
    tuf_context_t *ctx;
    sixel_frame_t wrong;
    int ok;

    ctx = context;
    ++ctx->calls;
    ok = tuf_check(frame, ctx->variant);
    if (ok && ctx->variant == 14) {
        /* Keep the same area and pixels so only the dimension check rejects.
         * This negative control must fail if that check is removed. */
        wrong = *frame;
        wrong.width /= 2;
        wrong.height *= 2;
        ok = !tuf_check(&wrong, ctx->variant);
    }
    return ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
tuf_trace_control(void)
{
    sixel_cms_profile_t *profile;
    sixel_cms_transform_t *transform;
    float input[3] = {0.25f, 0.5f, 0.75f};
    float output[3];
    int ok;

    profile = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(profile,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, profile,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, 0);
    ok = transform != NULL &&
        sixel_cms_do_transform(transform, input, output, 1u);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(profile);
    return ok;
}
#endif

int
tiff_float_values_test(int argc, char **argv, int variant)
{
#if HAVE_LIBTIFF
    sixel_loader_t *loader;
    sixel_cms_engine_t previous;
    tuf_context_t context;
    SIXELSTATUS status;
    char order[128];
    char const *target;
    int control;
    int result;

    if (argc != 2 || variant < 0 || variant > 17) { return 2; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    control = tuf_trace_control();
    fprintf(stderr, "TFLOAT|control=%d|case-begin\n", control);
    target = variant == 15 ? "gamma" : variant == 17 ? "oklab" : "linear";
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=%s:prefer_8bit=%d!",
        variant == 10 ? "none" : "builtin", target, variant == 16);
    loader = NULL;
    context.variant = variant;
    context.calls = 0;
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
        status = sixel_loader_load_file(loader, argv[1], tuf_capture);
    }
    result = control && status == SIXEL_OK && context.calls == 1;
    fprintf(stderr, "TFLOAT|status=%x|callbacks=%d\n", status, context.calls);
    if (loader != NULL) { sixel_loader_unref(loader); }
    sixel_cms_set_engine(previous);
    return result ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    (void)variant;
    return 77;
#endif
}
