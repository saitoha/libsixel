/* SPDX-License-Identifier: MIT */
/* The public loader is checked against independently decoded device samples
 * and Decimal PCS values, never another libsixel/CMM RGB result. */
#include "config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/compat_stub.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "jpeg_cmyk_test_common.h"

#if HAVE_JPEG
#include <jpeglib.h>
#include "tests/data/color-numeric/jpeg-cmyk.h"
#include "tests/data/color-numeric/jpeg-precision.h"

typedef struct jpeg_cmyk_context {
    int adobe;
    int extended;
    int fixture;
    int engine;
    int mode;
    int calls;
} jpeg_cmyk_context_t;

static SIXELSTATUS
jc_capture(sixel_frame_t *frame, void *context)
{
    jpeg_cmyk_context_t *ctx;
    float const *pixels;
    double expected;
    double bound;
    double value;
    size_t i;
    size_t c;
    int disabled;
    int dimension;
    size_t count;
    size_t witness;

    ctx = context;
    ++ctx->calls;
    disabled = ctx->mode == 1;
    dimension = ctx->extended ? 64 : 32;
    count = (size_t)dimension * dimension;
    if (frame->width != dimension || frame->height != dimension ||
        frame->pixelformat != (disabled ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                              SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (disabled ? SIXEL_COLORSPACE_GAMMA :
                             SIXEL_COLORSPACE_LINEAR) ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u) {
        fprintf(stderr, "JCMYK|metadata=0|format=%x|space=%d\n",
                frame->pixelformat, frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    if (pixels == NULL) { return SIXEL_BAD_INPUT; }
    /* Geometry above fixes the complete source extent. Every component is
     * visited, including finite checks; no paired short iterator is used. */
    witness = 0u;
    for (i = 0u; i < count; ++i) {
        for (c = 0u; c < 3u; ++c) {
            if (!isfinite(pixels[i * 3u + c])) { return SIXEL_BAD_INPUT; }
            if (ctx->extended && (witness >= 32u ||
                                  i != jp_witness[witness])) {
                continue;
            }
            if (ctx->extended) {
                expected = ctx->mode == 0 ?
                    jp_linear[ctx->fixture][witness][c] :
                    jp_fallback[ctx->fixture][!disabled][witness][c];
            } else {
                expected = ctx->mode == 0 ? jc_linear[ctx->adobe][i][c] :
                           ctx->mode == 5 ? jc_legacy[i][c] :
                           jc_fallback[!disabled][i][c];
            }
            bound = fabs(expected) * jc_rounding;
            if (ctx->mode == 0 && ctx->engine != SIXEL_CMS_ENGINE_BUILTIN) {
                bound += jc_pcs_bound[c];
            }
            value = pixels[i * 3u + c];
            if (!isfinite(value) || value < expected - bound ||
                value > expected + bound) {
                fprintf(stderr, "JCMYK|pixel=%zu|channel=%zu|value=%.17g|"
                    "expected=%.17g|bound=%.17g\n", i, c, value,
                    expected, bound);
                return SIXEL_BAD_INPUT;
            }
        }
        if (ctx->extended && witness < 32u && i == jp_witness[witness]) {
            ++witness;
        }
    }
    if (ctx->extended && witness != 32u) { return SIXEL_BAD_INPUT; }
    fprintf(stderr, "JCMYK|samples=%zu|finite=1|interval=1\n",
        (ctx->extended ? witness : count) * 3u);
    return SIXEL_OK;
}

static int
jc_load(char const *path, char const *order, jpeg_cmyk_context_t *context)
{
    sixel_loader_t *loader;
    SIXELSTATUS status;
    int no;

    loader = NULL;
    no = 0;
    context->calls = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                      context);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
                                      &no);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                      order);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_loader_load_file(loader, path, jc_capture);
    }
    sixel_loader_unref(loader);
    return status == SIXEL_OK && context->calls == 1;
}
#endif

int
jpeg_cmyk_values(int argc, char **argv, int adobe, int engine, int mode)
{
#if HAVE_JPEG
    static char const *const kinds[] = {
        "affine", "affine", "absent", "rejected", "missing-intent"
    };
    sixel_cms_engine_t previous;
    jpeg_cmyk_context_t context;
    char path[4096];
    char order[160];
    char const *suffix;
    int length;
    int ok;

    if (argc != 2) { return 2; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    ok = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine(previous);
    if (!ok) { return 77; }
    context.extended = 0;
    context.fixture = 0;
    context.adobe = adobe;
    context.engine = engine;
    suffix = adobe ? "adobe" : "plain";
    snprintf(order, sizeof(order),
        "libjpeg:cms_engine=%s:cms_intent=relative!!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
    /* A successful real transform on the same coding samples qualifies
     * both trace observation and the later apply failure/rejection. */
    if (mode == 4 || mode == 6 || mode == 7) {
        length = snprintf(path, sizeof(path),
            "%s/tests/data/color-numeric/jpeg-cmyk-affine-%s.jpg",
            argv[1], suffix);
        if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
        context.mode = 0;
        if (!jc_load(path, order, &context)) { return 1; }
    }
    if (mode >= 5) {
        length = snprintf(path, sizeof(path),
            "%s/tests/data/colormgmt/input/custom/cmyk_mft2_valid.jpg",
            argv[1]);
        if (mode != 6) {
            snprintf(order, sizeof(order), "libjpeg:cms_engine=%s!",
                sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
        }
    } else {
        length = snprintf(path, sizeof(path),
            "%s/tests/data/color-numeric/jpeg-cmyk-%s-%s.jpg",
            argv[1], kinds[mode], suffix);
    }
    if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
    context.mode = mode;
    fprintf(stderr, "JCMYK|target-begin\n");
    ok = jc_load(path, order, &context);
    return ok ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    (void)adobe;
    (void)engine;
    (void)mode;
    return 77;
#endif
}

int
jpeg_cmyk_source(int argc, char **argv)
{
#if HAVE_JPEG
    static char const *const kinds[] = {
        "affine", "absent", "rejected", "missing-intent"
    };
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    JSAMPARRAY row;
    FILE *file;
    char path[4096];
    unsigned int y;
    int variant;
    int adobe;
    int length;
    int ok;

    if (argc != 2) { return 2; }
    /* Check the original and all metadata-only variants independently of
     * the production loader/CMM, including header polarity and all bytes. */
    for (variant = 0; variant < 9; ++variant) {
        adobe = variant == 8 || variant % 2;
        if (variant == 8) {
            length = snprintf(path, sizeof(path),
                "%s/tests/data/colormgmt/input/custom/cmyk_mft2_valid.jpg",
                argv[1]);
        } else {
            length = snprintf(path, sizeof(path),
                "%s/tests/data/color-numeric/jpeg-cmyk-%s-%s.jpg",
                argv[1], kinds[variant / 2], adobe ? "adobe" : "plain");
        }
        if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
        file = sixel_compat_fopen(path, "rb");
        if (file == NULL) { return 1; }
        decoder.err = jpeg_std_error(&error);
        jpeg_create_decompress(&decoder);
        jpeg_stdio_src(&decoder, file);
        jpeg_read_header(&decoder, TRUE);
        ok = decoder.data_precision == 8 &&
             decoder.jpeg_color_space == JCS_CMYK &&
             !!decoder.saw_Adobe_marker == adobe &&
             (!adobe || decoder.Adobe_transform == 0);
        decoder.out_color_space = JCS_CMYK;
        decoder.dct_method = JDCT_ISLOW;
        jpeg_start_decompress(&decoder);
        ok = ok && decoder.output_width == 32u &&
             decoder.output_height == 32u && decoder.output_components == 4;
        if (ok) {
            row = (*decoder.mem->alloc_sarray)((j_common_ptr)&decoder,
                JPOOL_IMAGE, 128u, 1u);
            while (decoder.output_scanline < 32u) {
                y = decoder.output_scanline;
                ok &= jpeg_read_scanlines(&decoder, row, 1u) == 1u;
                ok &= memcmp(row[0], jc_raw[y * 32u], 128u) == 0;
            }
            jpeg_finish_decompress(&decoder);
        }
        jpeg_destroy_decompress(&decoder);
        fclose(file);
        if (!ok) { return 1; }
    }
    return 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}

/* One selected precision/engine/failure observation per TAP invocation. */
int
test_jc_precision(int argc, char **argv)
{
#if HAVE_JPEG
    static char const *const names[] = {
        "8bit-ycck-seq444", "12bit-cmyk-seq444",
        "12bit-ycck-seq444", "16bit-cmyk-lossless"
    };
    static char const *const kinds[] = {
        "affine", "affine", "absent", "rejected", "missing-intent"
    };
    jpeg_cmyk_context_t context;
    sixel_cms_engine_t previous;
    char path[4096];
    char order[160];
    int fixture;
    int engine;
    int mode;
    int length;
    int available;

    if (argc != 5) { return 2; }
    fixture = atoi(argv[2]);
    engine = atoi(argv[3]);
    mode = atoi(argv[4]);
    if (fixture < 0 || fixture > 3 || mode < 0 || mode > 4) { return 2; }
#if !HAVE_JPEG12_API
    if (fixture == 1 || fixture == 2) { return 77; }
#endif
#if !HAVE_JPEG16_API
    if (fixture == 3) { return 77; }
#endif
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    available = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine(previous);
    if (!available) { return 77; }
    context.adobe = 1;
    context.extended = 1;
    context.fixture = fixture;
    context.engine = engine;
    snprintf(order, sizeof(order),
        "libjpeg:cms_engine=%s:cms_intent=relative!!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
    if (mode == 4) {
        length = snprintf(path, sizeof(path),
            "%s/tests/data/color-numeric/jpeg-precision-%s-affine.jpg",
            argv[1], names[fixture]);
        if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
        context.mode = 0;
        if (!jc_load(path, order, &context)) { return 1; }
    }
    length = snprintf(path, sizeof(path),
        "%s/tests/data/color-numeric/jpeg-precision-%s-%s.jpg",
        argv[1], names[fixture], kinds[mode]);
    if (length < 0 || (size_t)length >= sizeof(path)) { return 2; }
    context.mode = mode;
    fprintf(stderr, "JCMYK|target-begin\n");
    return jc_load(path, order, &context) ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
