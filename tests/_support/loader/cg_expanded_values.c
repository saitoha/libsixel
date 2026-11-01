/* SPDX-License-Identifier: MIT */
/* Independent source values, alpha arithmetic and allocation contracts. */
#include <math.h>
#include <stdint.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/png-samples.h"
#include "src/cms.h"
#include "src/loader-common.h"

#if HAVE_COREGRAPHICS
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#define CGE_COUNT 15u
#define CGE_FLOAT_BYTES (CGE_COUNT * 3u * sizeof(float))

typedef struct {
    int colors;
    int alpha;
    int srgb;
    int prefer;
    int background;
    int policy;
    int reference;
    int calls;
    double linear[CGE_COUNT * 3u];
} cge_probe_t;

static int cge_live;
static int cge_float_allocations;
static size_t cge_fail_size;

static void *
cge_malloc(size_t size)
{
    void *p;

    if (size == CGE_FLOAT_BYTES) { ++cge_float_allocations; }
    if (size == cge_fail_size) { cge_fail_size = 0u; return NULL; }
    p = malloc(size);
    if (p != NULL) { ++cge_live; }
    return p;
}

static void
cge_free(void *p)
{
    if (p != NULL) { --cge_live; }
    free(p);
}

static void *
cge_calloc(size_t n, size_t size)
{
    void *p;

    if (size != 0u && n > SIZE_MAX / size) { return NULL; }
    p = cge_malloc(n * size);
    if (p != NULL) { memset(p, 0, n * size); }
    return p;
}

static void *
cge_realloc(void *p, size_t size)
{
    void *q;
    int was_null;

    if (size == 0u) { cge_free(p); return NULL; }
    was_null = p == NULL;
    q = realloc(p, size);
    if (q != NULL && was_null) { ++cge_live; }
    return q;
}

static int
cge_byte_path(cge_probe_t const *probe)
{
    return probe->srgb && !probe->prefer && !probe->reference &&
        (!probe->background || !probe->alpha ||
         (probe->alpha == 1 && probe->policy != SIXEL_ALPHA_POLICY_COMPOSITE));
}

static SIXELSTATUS
cge_capture(sixel_frame_t *frame, void *context)
{
    static unsigned char const partial[8] =
        {255, 128, 0, 255, 64, 255, 255, 193};
    cge_probe_t *probe;
    int byte_path;
    int preserve;
    int slot;
    int p;
    int c;
    double expected;
    double actual;
    double alpha;
    double background;
    double tolerance;

    probe = (cge_probe_t *)context;
    ++probe->calls;
    byte_path = cge_byte_path(probe);
    preserve = probe->alpha && (probe->reference || !probe->background ||
        probe->policy != SIXEL_ALPHA_POLICY_COMPOSITE);
    if (frame->width != 5 || frame->height != 3 ||
        frame->pixelformat != (byte_path ? SIXEL_PIXELFORMAT_RGB888 :
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (byte_path ? SIXEL_COLORSPACE_GAMMA :
            SIXEL_COLORSPACE_LINEAR) || !frame->owns_pixels ||
        frame->transparent != -1 || frame->ncolors != -1 ||
        frame->palette != NULL ||
        (frame->transparent_mask != NULL) != preserve ||
        frame->transparent_mask_size != (preserve ? CGE_COUNT : 0u) ||
        frame->alpha_zero_is_transparent != preserve) {
        fprintf(stderr, "CG expanded frame format=%x mask=%d flag=%d\n",
                frame->pixelformat, frame->transparent_mask != NULL,
                frame->alpha_zero_is_transparent);
        return SIXEL_BAD_INPUT;
    }
    for (p = 0; p < (int)CGE_COUNT; ++p) {
        slot = p % probe->colors;
        alpha = !probe->alpha ? 1.0 : probe->alpha == 1 ?
            (slot == 2 ? 0.0 : 1.0) : partial[slot] / 255.0;
        if (preserve && frame->transparent_mask[p] != (alpha == 0.0)) {
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            expected = probe->srgb ? color_png_srgb_linear[0][slot][c] :
                                    color_png_icc_linear[0][slot][c];
            tolerance = 0.0001;
            if (byte_path) {
                expected = color_png_source[0][slot][c];
                actual = frame->pixels.u8ptr[p * 3 + c];
                tolerance = 0.0;
            } else {
                actual = frame->pixels.f32ptr[p * 3 + c];
            }
            if (probe->background && !probe->reference && !byte_path) {
                /* First validate the native transform independently, then
                 * isolate exact original-alpha and background arithmetic. */
                expected = probe->linear[p * 3 + c];
                tolerance = 0.0000005;
                if (!(preserve && alpha == 0.0)) {
                    background = 64.0 * (c + 1) / 255.0;
                    if (probe->background == 1) {
                        background = pow((background + 0.055) / 1.055, 2.4);
                    }
                    expected = expected * alpha + background * (1.0 - alpha);
                }
            }
            if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
                fprintf(stderr, "CG expanded pixel=%d channel=%d "
                        "%.12g != %.12g\n", p, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
            if (probe->reference) { probe->linear[p * 3 + c] = actual; }
        }
    }
    return SIXEL_OK;
}

static int
cge_provider_expanded(unsigned char const *bytes, size_t size)
{
    CFDataRef data;
    CGImageSourceRef source;
    CGImageRef image;
    CGColorSpaceRef space;
    int result;

    result = 1;
    data = CFDataCreate(NULL, bytes, (CFIndex)size);
    source = data == NULL ? NULL : CGImageSourceCreateWithData(data, NULL);
    image = source == NULL ? NULL :
        CGImageSourceCreateImageAtIndex(source, 0, NULL);
    space = image == NULL ? NULL : CGImageGetColorSpace(image);
    if (space != NULL) {
        /* Provider layout is OS-owned. An indexed provider exercises the
         * separately tested palette route, not this expanded-RGB contract. */
        result = CGColorSpaceGetModel(space) == kCGColorSpaceModelIndexed ?
            77 : CGColorSpaceGetModel(space) != kCGColorSpaceModelRGB;
    }
    if (image != NULL) { CGImageRelease(image); }
    if (source != NULL) { CFRelease(source); }
    if (data != NULL) { CFRelease(data); }
    return result;
}
#endif

int
test_cg_expanded_values(int argc, char **argv)
{
#if HAVE_COREGRAPHICS
    unsigned char bytes[4096];
    unsigned char bgcolor[3] = {64, 128, 192};
    size_t size;
    cge_probe_t probe;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    sixel_cms_engine_t previous_engine;
    int previous_policy;
    int fault;
    int result;

    if (argc != 10) { return 1; }
    if (edge_read_fixture(argv[1], bytes, sizeof(bytes), &size) != 0) {
        return 1;
    }
    result = cge_provider_expanded(bytes, size);
    if (result != 0) { return result; }
    memset(&probe, 0, sizeof(probe));
    probe.colors = atoi(argv[2]) < 3 ? 1 << atoi(argv[2]) : 8;
    probe.alpha = atoi(argv[3]);
    probe.srgb = atoi(argv[4]);
    probe.prefer = atoi(argv[5]);
    probe.background = atoi(argv[6]);
    probe.policy = atoi(argv[7]);
    fault = atoi(argv[8]);
    allocator = NULL;
    chunk = NULL;
    loader = NULL;
    previous_engine = sixel_cms_get_engine();
    previous_policy = loader_transparent_policy();
    sixel_cms_set_engine(atoi(argv[9]) ? SIXEL_CMS_ENGINE_NONE :
                                        SIXEL_CMS_ENGINE_AUTO);
    sixel_helper_set_loader_transparent_policy(probe.policy);
    cge_live = 0;
    cge_fail_size = 0u;
    cge_float_allocations = 0;
    status = sixel_allocator_new(&allocator, cge_malloc, cge_calloc,
                                  cge_realloc, cge_free);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_chunk_create_from_memory(&chunk, bytes, size,
                                            NULL, allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
        probe.background == 2 ? "coregraphics:background_colorspace=linear!" :
                                "coregraphics:background_colorspace=gamma!");
    if (SIXEL_FAILED(status)) { goto end; }
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    if (probe.background) {
        probe.reference = 1;
        sixel_loader_set_prefer_float32(loader, 1);
        status = sixel_loader_load_chunk(loader, chunk, cge_capture);
        if (SIXEL_FAILED(status)) { goto end; }
        probe.reference = 0;
        (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_BGCOLOR, bgcolor);
    }
    sixel_loader_set_prefer_float32(loader, probe.prefer);
    cge_float_allocations = 0;
    cge_fail_size = fault == 1 ? CGE_FLOAT_BYTES : fault == 2 ? CGE_COUNT : 0u;
    fprintf(stderr, "CGE_FINAL\n");
    status = sixel_loader_load_chunk(loader, chunk, cge_capture);
    if (fault) {
        if (status != SIXEL_BAD_ALLOCATION || probe.calls != 0 ||
            cge_fail_size != 0u) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        status = sixel_loader_load_chunk(loader, chunk, cge_capture);
    }
    if (SIXEL_SUCCEEDED(status) &&
        (probe.calls != (probe.background ? 2 : 1) ||
         (cge_byte_path(&probe) && cge_float_allocations != 0))) {
        status = SIXEL_BAD_INPUT;
    }
end:
    cge_fail_size = 0u;
    sixel_loader_unref(loader);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(previous_engine);
    sixel_helper_set_loader_transparent_policy(previous_policy);
    if (SIXEL_FAILED(status) || cge_live != 0) {
        fprintf(stderr, "CG expanded status=%x calls=%d live=%d: %s\n",
                status, probe.calls, cge_live,
                sixel_helper_get_additional_message());
        return 1;
    }
    return 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
