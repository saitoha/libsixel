/* SPDX-License-Identifier: MIT */
/* Public-manager observations use fixed offline source/ICC values. */
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/colorspace.h"
#include "src/loader.h"
#include "tests/data/color-numeric/tiff-byte.h"
#include "tiff_byte_test_common.h"

typedef struct tb_context {
    int fixture;
    int engine;
    /* Modes: 0 canonical, 1 all-byte fallback, 2 gamma, 3 final byte,
     * 4 six-pixel rollback, 5 orientation/VOID driver, 6 target control,
     * 7 capture canonical values for the existing converter control. */
    int mode;
    int orientation;
    int calls;
    int target_format;
    int target_space;
    float control[18];
} tb_context_t;

static int
tb_check(sixel_frame_t const *frame, tb_context_t const *ctx)
{
    float const *pixels;
    double const *expected;
    double relative;
    double magnitude;
    double allowance;
    size_t i;
    size_t c;
    size_t mapped;
    size_t x;
    size_t y;
    size_t n;
    int width;
    int height;
    int format;
    int space;
    int byte;
    unsigned char wanted;

    width = ctx->mode == 1 ? 256 : ctx->orientation ? 3 : 6;
    height = ctx->orientation ? 2 : 1;
    byte = ctx->mode == 1 || ctx->mode == 3 || ctx->mode == 4;
    format = byte ? SIXEL_PIXELFORMAT_RGB888 : ctx->mode == 2
        ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    space = byte || ctx->mode == 2 ? SIXEL_COLORSPACE_GAMMA
                                  : SIXEL_COLORSPACE_LINEAR;
    if (ctx->mode == 6) {
        format = ctx->target_format;
        space = ctx->target_space;
    }
    if (frame->width != width || frame->height != height ||
        frame->pixelformat != format || frame->colorspace != space ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL || frame->transparent_mask_size ||
        frame->alpha_zero_is_transparent) { return 0; }
    if (ctx->mode == 6) {
        return memcmp(frame->pixels.u8ptr, ctx->control,
                      sizeof(ctx->control)) == 0;
    }
    n = (size_t)width * height;
    pixels = (float const *)frame->pixels.u8ptr;
    expected = ctx->mode == 2 ? tb_cases[ctx->fixture].gamma
                             : tb_cases[ctx->fixture].expected;
    relative = ctx->engine == 4 ? tb_native_relative : tb_float_relative;
    for (i = 0u; i < n; ++i) {
        x = i % width;
        y = i / width;
        if (ctx->orientation == 2 || ctx->orientation == 3) {
            x = (size_t)width - 1u - x;
        }
        if (ctx->orientation == 3 || ctx->orientation == 4) {
            y = (size_t)height - 1u - y;
        }
        mapped = y * width + x;
        magnitude = byte ? 0.0 : fmax(fabs(expected[mapped * 3u]),
            fmax(fabs(expected[mapped * 3u + 1u]),
                 fabs(expected[mapped * 3u + 2u])));
        allowance = magnitude * relative;
        for (c = 0u; c < 3u; ++c) {
            if (ctx->mode == 1) {
                wanted = tb_cases[ctx->fixture].photo == 2
                    ? (unsigned char)(c == 0u
                    ? i : c == 1u ? (73u * i + 17u) % 256u : 255u - i)
                    : (unsigned char)(ctx->fixture == 1 ? i : 255u - i);
            } else if (ctx->mode == 4) {
                wanted = tb_cases[ctx->fixture].stored[
                    tb_cases[ctx->fixture].photo == 2
                    ? mapped * 3u + c : mapped];
                if (tb_cases[ctx->fixture].photo == 0) {
                    wanted = 255u - wanted;
                }
            } else {
                wanted = tb_cases[ctx->fixture].byte[mapped * 3u + c];
            }
            if (byte ? frame->pixels.u8ptr[i * 3u + c] != wanted :
                !isfinite(pixels[i * 3u + c]) ||
                fabs(pixels[i * 3u + c] - expected[mapped * 3u + c]) >
                    allowance) {
                fprintf(stderr, "TBYTE|pixel=%zu|channel=%zu|mode=%d\n",
                        i, c, ctx->mode);
                return 0;
            }
        }
    }
    return 1;
}

int
tiff_byte_check(sixel_frame_t const *frame, int fixture, int engine, int raw)
{
    tb_context_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.fixture = fixture;
    ctx.engine = engine;
    ctx.mode = raw ? 4 : 0;
    return tb_check(frame, &ctx);
}

#if HAVE_LIBTIFF
static SIXELSTATUS
tb_capture(sixel_frame_t *frame, void *context)
{
    tb_context_t *ctx;

    ctx = context;
    ++ctx->calls;
    if (ctx->mode == 7 && tb_check(frame, ctx)) {
        memcpy(ctx->control, frame->pixels.u8ptr, sizeof(ctx->control));
    }
    return tb_check(frame, ctx) ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static int
tb_load(char const *directory, tb_context_t *ctx, char const *suffix,
        char const *target, int prefer, int background)
{
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char path[4096];
    char order[128];
    unsigned char bg[3];
    int ok;

    loader = NULL;
    ctx->calls = 0;
    bg[0] = 19; bg[1] = 43; bg[2] = 71;
    snprintf(path, sizeof(path), "%s/tiff-byte-%s-%s.tif", directory,
             tb_cases[ctx->fixture].name, suffix);
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=%s:prefer_8bit=%d!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)ctx->engine),
        target, prefer);
    fprintf(stderr, "TBYTE|load-begin|background=%d\n", background);
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, ctx);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     order);
    }
    if (SIXEL_SUCCEEDED(status) && background) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR, bg);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_load_file(loader, path, tb_capture);
    }
    ok = status == SIXEL_OK && ctx->calls == 1;
    if (!ok) {
        fprintf(stderr, "TBYTE|file=%s|status=%x|calls=%d\n",
                path, status, ctx->calls);
    }
    sixel_loader_unref(loader);
    fprintf(stderr, "TBYTE|load-end|ok=%d\n", ok);
    return ok;
}
#endif

int
tiff_byte_test(int argc, char **argv, int fixture, int engine, int mode)
{
#if HAVE_LIBTIFF
    sixel_cms_engine_t previous;
    tb_context_t ctx;
    char const *suffixes[] = { "all256-none", "all256-invalid",
                              "all256-mismatch", "all256-icc" };
    char suffix[32];
    int ok;
    int background;
    int i;
    int sample;
    char const *targets[] = { "cielab", "oklab", "din99d" };
    int const formats[] = { SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32 };
    int const spaces[] = { SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_OKLAB,
                          SIXEL_COLORSPACE_DIN99D };

    if (argc != 2) { return 2; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    memset(&ctx, 0, sizeof(ctx));
    ctx.fixture = fixture;
    ctx.engine = engine;
    ctx.mode = mode == 5 ? 0 : mode;
    ok = 1;
    for (background = 0; ok && background < 2; ++background) {
        if (mode == 1) {
            /* None must ignore even a valid embedded source profile. */
            for (i = 0; ok && i < 4; ++i) {
                ctx.engine = SIXEL_CMS_ENGINE_NONE;
                ok = tb_load(argv[1], &ctx, suffixes[i], "oklab", 1,
                             background);
                if (ok && i < 3) {
                    ctx.engine = 2;
                    ok = tb_load(argv[1], &ctx, suffixes[i], "oklab", 1,
                                 background);
                }
            }
        } else if (mode == 5) {
            for (ctx.fixture = 0; ok && ctx.fixture < 3; ++ctx.fixture) {
                for (ctx.orientation = 1; ok && ctx.orientation <= 4;
                     ++ctx.orientation) {
                    for (sample = 1; ok && sample <= 4; sample += 3) {
                        snprintf(suffix, sizeof(suffix), "o%d-s%d",
                                 ctx.orientation, sample);
                        ok = tb_load(argv[1], &ctx, suffix, "linear", 0,
                                     background);
                    }
                }
            }
        } else if (mode == 2 || mode == 3) {
            for (ctx.fixture = 0; ok && ctx.fixture < 3; ++ctx.fixture) {
                ok = tb_load(argv[1], &ctx, "icc",
                             mode == 2 ? "gamma" : "linear", mode == 3,
                             background);
            }
        } else {
            ok = tb_load(argv[1], &ctx, "icc",
                         mode == 2 ? "gamma" : "linear", mode == 3,
                         background);
        }
    }
    if (ok && mode == 2) {
        /* These are converter/metadata controls, separate from the fixed
         * independent gamma oracle above; do not define new CMS budgets. */
        ctx.fixture = 0;
        for (i = 0; ok && i < 3; ++i) {
            ctx.mode = 7;
            ok = tb_load(argv[1], &ctx, "icc", "linear", 0, 0);
            ctx.mode = 6;
            ctx.target_format = formats[i];
            ctx.target_space = spaces[i];
            ok = ok && sixel_color_convert_rgbf32(ctx.control, 6u,
                SIXEL_COLORSPACE_LINEAR, spaces[i]) == SIXEL_OK;
            ok = ok && tb_load(argv[1], &ctx, "icc", targets[i], 0, 0);
        }
    }
    fprintf(stderr, "TBYTE|ok=%d\n", ok);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
#else
    (void)argc; (void)argv; (void)fixture; (void)engine; (void)mode;
    return 77;
#endif
}
