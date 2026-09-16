/* SPDX-License-Identifier: MIT */
/* Independent original-sample/ICC tables qualify canonical values first.
 * Subsequent controls isolate composition and final publication from CMS. */
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/colorspace.h"
#include "src/loader.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/tiff-alpha.h"
#include "tiff_alpha_test_common.h"

#if HAVE_LIBTIFF
typedef struct at_context {
    int fixture;
    int engine;
    int managed;
    int background;
    int keep;
    int orientation;
    int calls;
    int capture;
    int format;
    int space;
    int target;
    float canonical[30];
    float converted[30];
} at_context_t;

/* These formulas use long-double intermediates and the mathematical sRGB
 * definition, without calling the production transfer/composition helpers. */
static long double
at_transfer(long double value, int encode)
{
    long double magnitude;
    long double result;

    magnitude = fabsl(value);
    if (encode) {
        result = magnitude <= 0.0031308L ? magnitude * 12.92L
            : 1.055L * powl(magnitude, 5.0L / 12.0L) - 0.055L;
    } else {
        result = magnitude <= 0.04045L ? magnitude / 12.92L
            : powl((magnitude + 0.055L) / 1.055L, 2.4L);
    }
    return value < 0.0L ? -result : result;
}

static int
at_near_word(float actual, float expected)
{
    uint32_t a;
    uint32_t b;

    if (!isfinite(actual) || !isfinite(expected)) { return 0; }
    memcpy(&a, &actual, sizeof(a));
    memcpy(&b, &expected, sizeof(b));
    return a > b ? a - b <= 1u : b - a <= 1u;
}

static float
at_composed(at_context_t const *ctx, size_t pixel, size_t channel)
{
    unsigned char const background[] = { 19u, 43u, 71u };
    unsigned int alpha;
    unsigned int maximum;
    long double a;
    long double b;
    long double value;

    maximum = ta_cases[ctx->fixture].bits == 16 ? 65535u : 255u;
    alpha = ta_cases[ctx->fixture].alpha[pixel];
    value = ctx->canonical[pixel * 3u + channel];
    if (ctx->background && alpha < maximum && (alpha || !ctx->keep)) {
        /* Preserve the specified binary64 alpha/background input rounding,
         * then independently round the composed result once to binary32. */
        a = (double)alpha / maximum;
        b = (double)background[channel] / 255.0;
        if (ctx->background == 1) { b = at_transfer(b, 0); }
        value = a * value + (1.0L - a) * b;
    }
    return (float)value;
}

static SIXELSTATUS
at_capture(sixel_frame_t *frame, void *context)
{
    at_context_t *ctx;
    float actual;
    float expected;
    double magnitude;
    double allowance;
    double value;
    long double byte_value;
    uint32_t word;
    size_t i;
    size_t c;
    size_t mapped;
    size_t count;
    size_t x;
    size_t y;
    int masked;
    int want_mask;
    int byte;
    int expected_byte;
    int ok;

    ctx = context;
    ++ctx->calls;
    count = (size_t)ta_cases[ctx->fixture].count;
    byte = ctx->format == SIXEL_PIXELFORMAT_RGB888;
    masked = !ta_cases[ctx->fixture].opaque &&
        (!ctx->background || ctx->keep);
    ok = frame->width == (ctx->orientation ? 2 : (int)count) &&
        frame->height == (ctx->orientation ? 2 : 1) &&
        frame->pixelformat == ctx->format && frame->colorspace == ctx->space &&
        frame->ncolors == -1 && frame->transparent == -1 &&
        frame->transparent_mask_size == (masked ? count : 0u) &&
        (frame->transparent_mask != NULL) == masked &&
        frame->alpha_zero_is_transparent == masked;
    if (!ok) { return SIXEL_BAD_INPUT; }
    for (i = 0u; i < count; ++i) {
        x = i % 2u;
        y = i / 2u;
        if (ctx->orientation == 2 || ctx->orientation == 3) { x = 1u - x; }
        if (ctx->orientation == 3 || ctx->orientation == 4) { y = 1u - y; }
        mapped = ctx->orientation ? y * 2u + x : i;
        want_mask = masked && ta_cases[ctx->fixture].alpha[mapped] == 0u;
        if (masked && frame->transparent_mask[i] != want_mask) { ok = 0; }
        magnitude = 0.0;
        for (c = 0u; c < 3u; ++c) {
            value = ta_cases[ctx->fixture].canonical[ctx->engine == 2 ? 0 : 1]
                [mapped * 3u + c];
            magnitude = fmax(magnitude, fabs(value));
        }
        allowance = magnitude * (ctx->engine == 4
            ? 0.00032384445817172027 : 0.000003814697265625);
        for (c = 0u; c < 3u; ++c) {
            if (ctx->managed && ctx->capture) {
                actual = ((float *)frame->pixels.u8ptr)[i * 3u + c];
                value = ta_cases[ctx->fixture].canonical[
                    ctx->engine == 2 ? 0 : 1][mapped * 3u + c];
                if (!isfinite(actual) || fabs(actual - value) > allowance) {
                    ok = 0;
                }
                ctx->canonical[i * 3u + c] = actual;
                continue;
            }
            if (ctx->managed) {
                expected = at_composed(ctx, mapped, c);
                byte_value = expected;
                if (ctx->target == 1) {
                    expected = (float)at_transfer(expected, 1);
                } else if (ctx->target == 2) {
                    /* Final byte rounding has no intermediate gamma F32. */
                    byte_value = at_transfer(expected, 1);
                } else if (ctx->target == 3) {
                    expected = ctx->converted[mapped * 3u + c];
                }
            } else {
                word = ta_cases[ctx->fixture].fallback[ctx->keep != 0]
                    [ctx->background][mapped * 3u + c];
                memcpy(&expected, &word, sizeof(expected));
                byte_value = expected;
            }
            if (byte) {
                expected_byte = (int)(fmaxl(0.0L, fminl(1.0L,
                    byte_value)) * 255.0L + 0.5L);
                if (frame->pixels.u8ptr[i * 3u + c] != expected_byte) {
                    ok = 0;
                }
            } else {
                memcpy(&actual, frame->pixels.u8ptr +
                    (i * 3u + c) * sizeof(float), sizeof(actual));
                if (ctx->managed && ctx->target != 3
                    ? !at_near_word(actual, expected)
                    : memcmp(&actual, &expected, sizeof(actual)) != 0) {
                    ok = 0;
                }
            }
        }
    }
    if (!ok) {
        fprintf(stderr, "TALPHA|frame-mismatch|fixture=%d|background=%d|"
            "target=%d|orientation=%d\n", ctx->fixture, ctx->background,
            ctx->target, ctx->orientation);
    }
    return ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

/* Pure-gamma Gray partitions unit and extended source without retrying.
 * Observe all original words in each bounded group at the native boundary. */
static void
at_trace_source(at_context_t const *ctx)
{
    char samples[2][81];
    size_t lengths[2];
    size_t i;
    size_t mapped;
    size_t x;
    size_t y;
    uint32_t word;
    int group;
    int groups;

    memset(samples, 0, sizeof(samples));
    lengths[0] = lengths[1] = 0u;
    groups = 0;
    if (ctx->managed && ctx->engine == 4 &&
        ta_cases[ctx->fixture].photo != 2) {
        for (i = 0u; i < (size_t)ta_cases[ctx->fixture].count; ++i) {
            x = i % 2u;
            y = i / 2u;
            if (ctx->orientation == 2 || ctx->orientation == 3) {
                x = 1u - x;
            }
            if (ctx->orientation == 3 || ctx->orientation == 4) {
                y = 1u - y;
            }
            mapped = ctx->orientation ? y * 2u + x : i;
            word = ta_cases[ctx->fixture].source[mapped * 3u];
            group = word > 0x3f800000u;
            snprintf(samples[group] + lengths[group], 9u, "%08x",
                     (unsigned int)word);
            lengths[group] += 8u;
        }
        groups = (lengths[0] != 0u) + (lengths[1] != 0u);
    }
    fprintf(stderr, "TALPHA16|applies=%d|groups=%d\n",
            ctx->managed ? (groups ? groups : 1) : 0, groups);
    for (group = 0; group < 2; ++group) {
        if (lengths[group] != 0u) {
            fprintf(stderr, "TALPHA16|source=%s\n", samples[group]);
        }
    }
}

static int
at_load(char const *directory, at_context_t *ctx, char const *suffix,
        char const *target, int prefer, int policy)
{
    sixel_loader_t *loader;
    SIXELSTATUS status;
    unsigned char background[] = { 19u, 43u, 71u };
    char path[4096];
    char order[128];
    int ok;

    loader = NULL;
    ctx->calls = 0;
    ctx->keep = policy != SIXEL_ALPHA_POLICY_COMPOSITE;
    ctx->format = ctx->managed ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
        : ta_cases[ctx->fixture].bits == 8 &&
          (ta_cases[ctx->fixture].opaque ||
           (!ta_cases[ctx->fixture].associated && !ctx->background))
        ? SIXEL_PIXELFORMAT_RGB888 : SIXEL_PIXELFORMAT_RGBFLOAT32;
    ctx->space = ctx->managed ? SIXEL_COLORSPACE_LINEAR
                             : SIXEL_COLORSPACE_GAMMA;
    if (ctx->managed && ctx->target == 1) {
        ctx->format = SIXEL_PIXELFORMAT_RGBFLOAT32;
        ctx->space = SIXEL_COLORSPACE_GAMMA;
    } else if (ctx->managed && ctx->target == 2) {
        ctx->format = SIXEL_PIXELFORMAT_RGB888;
        ctx->space = SIXEL_COLORSPACE_GAMMA;
    } else if (ctx->managed && ctx->target == 3) {
        ctx->space = strcmp(target, "oklab") == 0 ? SIXEL_COLORSPACE_OKLAB
            : strcmp(target, "cielab") == 0 ? SIXEL_COLORSPACE_CIELAB
                                            : SIXEL_COLORSPACE_DIN99D;
        ctx->format = ctx->space == SIXEL_COLORSPACE_OKLAB
            ? SIXEL_PIXELFORMAT_OKLABFLOAT32
            : ctx->space == SIXEL_COLORSPACE_CIELAB
            ? SIXEL_PIXELFORMAT_CIELABFLOAT32 : SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    }
    sixel_helper_set_loader_transparent_policy(policy);
    sixel_helper_set_loader_background_colorspace(ctx->background == 2
        ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA);
    snprintf(path, sizeof(path), "%s/tiff-alpha-%s-%s.tif", directory,
             ta_cases[ctx->fixture].name, suffix);
    snprintf(order, sizeof(order),
        "libtiff:cms_engine=%s:cms_target=%s:prefer_8bit=%d!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)ctx->engine),
        target, prefer);
    fprintf(stderr, "TALPHA|load-begin|engine=%s\n", ctx->managed
        ? sixel_cms_engine_to_string((sixel_cms_engine_t)ctx->engine) : "none");
    if (ta_cases[ctx->fixture].bits == 16) { at_trace_source(ctx); }
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, ctx);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     order);
    }
    if (SIXEL_SUCCEEDED(status) && ctx->background) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR,
                                     background);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_load_file(loader, path, at_capture);
    }
    ok = status == SIXEL_OK && ctx->calls == 1;
    if (!ok) {
        fprintf(stderr, "TALPHA|path=%s|status=%x|calls=%d\n",
                path, status, ctx->calls);
    }
    sixel_loader_unref(loader);
    fprintf(stderr, "TALPHA|load-end|ok=%d\n", ok);
    return ok;
}

static int
at_basis(char const *directory, at_context_t *ctx)
{
    int ok;

    ctx->background = 0;
    ctx->target = 0;
    ctx->orientation = 0;
    ctx->capture = 1;
    ok = at_load(directory, ctx, "icc", "linear", 0,
                 SIXEL_ALPHA_POLICY_COMPOSITE);
    ctx->capture = 0;
    return ok;
}
#endif

static int
at_test(int argc, char **argv, int photo, int engine, int mode, int bits)
{
#if HAVE_LIBTIFF
    at_context_t ctx;
    sixel_cms_engine_t previous;
    char const *suffixes[] = { "none", "invalid", "mismatch", "icc" };
    char const *targets[] = { "oklab", "cielab", "din99d" };
    int const spaces[] = { SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB,
                           SIXEL_COLORSPACE_DIN99D };
    int old_policy;
    int old_background;
    int ok;
    int association;
    int background;
    int kind;
    int opaque;
    int sample;
    int policy;
    int i;
    int start;
    int end;
    int offset;
    int saved_engine;
    int const orientations[] = { 4, 2, 2, 3, 3, 4 };
    char suffix[32];

    if (argc != 2) { return 2; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    old_policy = loader_transparent_policy();
    old_background = loader_background_colorspace();
    memset(&ctx, 0, sizeof(ctx));
    offset = bits == 16 ? 20 : 0;
    ctx.engine = engine;
    ctx.managed = mode != 1;
    ok = 1;
    if (mode == 0) {
        end = bits == 16 || photo == 0 ? 4 : 2;
        for (association = 0; ok && association < end; ++association) {
            ctx.fixture = offset + (association < 2 ? photo * 2 + association
                : bits == 16 ? 6 + photo * 2 + association - 2
                             : 6 + association - 2);
            if (bits == 16) {
                /* Exact fallback words detect premature source rounding,
                 * including 128/257, before any CMM error budget applies. */
                saved_engine = ctx.engine;
                ctx.engine = SIXEL_CMS_ENGINE_NONE;
                ctx.managed = 0;
                ctx.background = ctx.target = 0;
                ok = at_load(argv[1], &ctx, "icc", "linear", 0, 0);
                ctx.engine = saved_engine;
                ctx.managed = 1;
            }
            ok = ok && at_basis(argv[1], &ctx);
            for (background = 0; ok && background < 3; ++background) {
                ctx.background = background;
                ok = at_load(argv[1], &ctx, "icc", "linear", 0, 0);
            }
        }
    } else if (mode == 1) {
        for (opaque = 0; ok && opaque < 2; ++opaque) {
            for (association = 0; ok && association < 2; ++association) {
                ctx.fixture = offset + (opaque ? (bits == 16 ? 12 : 14)
                                              : 0) + photo * 2 + association;
                for (kind = 0; ok && kind < 4; ++kind) {
                    ctx.engine = kind == 3 ? SIXEL_CMS_ENGINE_NONE : 2;
                    for (background = 0; ok && background < 2; ++background) {
                        ctx.background = background;
                        ok = at_load(argv[1], &ctx, suffixes[kind],
                                     "oklab", 1, 0);
                    }
                }
                if (ok && bits == 16 && opaque && association == 1) {
                    /* The same opaque colors without ExtraSamples already
                     * have a high-depth float publication contract. */
                    ctx.engine = SIXEL_CMS_ENGINE_NONE;
                    ctx.background = 0;
                    ok = at_load(argv[1], &ctx, "noalpha", "linear", 0, 0);
                }
            }
        }
    } else if (mode == 2 && bits == 16) {
        for (i = 0; ok && i < 6; ++i) {
            ctx.fixture = offset + i;
            ok = at_basis(argv[1], &ctx);
            ctx.orientation = orientations[i];
            ok = ok && at_load(argv[1], &ctx, "layout", "linear", 0, 0);
        }
    } else if (mode == 2) {
        for (ctx.fixture = 0; ok && ctx.fixture < 6; ctx.fixture += 2) {
            ok = at_basis(argv[1], &ctx);
            for (ctx.orientation = 1; ok && ctx.orientation <= 4;
                 ++ctx.orientation) {
                for (sample = 1; ok && sample <= 4; sample += 3) {
                    snprintf(suffix, sizeof(suffix), "o%d-s%d",
                             ctx.orientation, sample);
                    ok = at_load(argv[1], &ctx, suffix, "linear", 0, 0);
                }
            }
        }
    } else if (mode == 3) {
        for (i = 0; ok && i < 2; ++i) {
            ctx.fixture = offset + (i == 0 ? 1 : 0);
            ctx.managed = i == 0;
            if (ctx.managed) { ok = at_basis(argv[1], &ctx); }
            for (background = 0; ok && background < 2; ++background) {
                ctx.background = background;
                for (policy = -1; ok && policy <= SIXEL_ALPHA_POLICY_AUTO;
                     ++policy) {
                    ok = at_load(argv[1], &ctx, i == 0 ? "icc" : "none",
                                 "linear", 0, policy);
                }
            }
        }
    } else {
        start = offset;
        end = offset + (bits == 16 ? 12 : mode == 6 ? 6 : 8);
        for (ctx.fixture = start; ok && ctx.fixture < end;
             ctx.fixture += mode == 6 ? 2 : 1) {
            ok = at_basis(argv[1], &ctx);
            for (background = 0; ok && background < 2; ++background) {
                ctx.background = background;
                ctx.target = mode == 4 ? 1 : mode == 5 ? 2 : 3;
                if (mode != 6) {
                    ok = at_load(argv[1], &ctx, "icc",
                        mode == 4 ? "gamma" : "linear", mode == 5, 0);
                } else {
                    for (kind = 0; ok && kind < 3; ++kind) {
                        ctx.keep = 0;
                        for (i = 0; i < ta_cases[ctx.fixture].count * 3; ++i) {
                            ctx.converted[i] = at_composed(&ctx,
                                (size_t)i / 3u, (size_t)i % 3u);
                        }
                        ok = sixel_color_convert_rgbf32(ctx.converted,
                            (size_t)ta_cases[ctx.fixture].count,
                            SIXEL_COLORSPACE_LINEAR, spaces[kind]) == SIXEL_OK;
                        ok = ok && at_load(argv[1], &ctx, "icc",
                            targets[kind], 0, 0);
                    }
                }
            }
        }
    }
    sixel_helper_set_loader_transparent_policy(old_policy);
    sixel_helper_set_loader_background_colorspace(old_background);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
#else
    (void)argc; (void)argv; (void)photo; (void)engine; (void)mode;
    (void)bits;
    return 77;
#endif
}

int
tiff_alpha_test(int argc, char **argv, int photo, int engine, int mode)
{
    return at_test(argc, argv, photo, engine, mode, 8);
}

int
tiff_alpha16_test(int argc, char **argv, int photo, int engine, int mode)
{
    return at_test(argc, argv, photo, engine, mode, 16);
}
