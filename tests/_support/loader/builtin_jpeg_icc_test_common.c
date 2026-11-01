/* SPDX-License-Identifier: MIT */
/* Constant JPEG coefficients/lossless samples and an affine ICC provide
 * independent device and RGB values without a runtime reference decoder. */
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/loader/builtin/loader_builtin_memory_test_common.h"
#include "builtin_jpeg_icc_test_common.h"
#include "tests/data/color-numeric/builtin-jpeg-icc.h"
#include "src/cms.h"

typedef struct {
    int fixture;
    int engine;
    int mode;
    int calls;
} bj_probe_t;

static SIXELSTATUS
bj_capture(sixel_frame_t *frame, void *context)
{
    bj_probe_t *p;
    unsigned char const *bytes;
    float const *pixels;
    size_t i;
    size_t c;
    size_t slot;
    int width;
    int height;
    int lossless;
    int byte_frame;
    int managed;
    double expected;
    double actual;
    double bound;
    double scale;

    p = context;
    ++p->calls;
    lossless = p->fixture >= 6 && p->fixture <= 8;
    width = lossless ? 5 : 17;
    height = lossless ? 3 : 9;
    managed = p->mode == 0;
    byte_frame = p->mode == 1 || p->mode == 2;
    if (frame->width != width || frame->height != height ||
        frame->pixelformat != (byte_frame ? SIXEL_PIXELFORMAT_RGB888 :
            managed ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
                      SIXEL_PIXELFORMAT_RGBFLOAT32) ||
        frame->colorspace != (managed ? SIXEL_COLORSPACE_LINEAR :
                                       SIXEL_COLORSPACE_GAMMA) ||
        frame->palette != NULL || frame->transparent_mask != NULL ||
        frame->transparent != -1) {
        fprintf(stderr, "BJICC metadata format=%x space=%d\n",
                frame->pixelformat, frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    bytes = sixel_frame_get_pixels(frame);
    pixels = (float const *)bytes;
    for (i = 0u; i < (size_t)width * (size_t)height; ++i) {
        slot = lossless ? i : i / (size_t)width / 8u * 3u +
                             i % (size_t)width / 8u;
        scale = 0.0;
        for (c = 0u; c < 3u; ++c) {
            scale = fmax(scale, fabs(bj_expected[p->fixture][slot][c]));
        }
        for (c = 0u; c < 3u; ++c) {
            expected = managed ? bj_expected[p->fixture][slot][c] :
                                 bj_fallback[slot][c];
            /* Builtin budget includes source float storage and the profile
             * matrix/CLUT arithmetic. Native CMMs may quantize XYZ u1.15;
             * the fixed profile's propagated PCS bound accounts for that.
             * Fallback uses the unchanged float multiply approximation. */
            bound = managed ? scale * 64.0 / 16777216.0 : 0.0000002;
            if (managed && p->engine != SIXEL_CMS_ENGINE_BUILTIN) {
                bound += bj_pcs_bound[c];
            }
            if (byte_frame) {
                expected = floor(expected * 255.0 + 0.5);
                bound = 0.0;
            }
            actual = byte_frame ? bytes[i * 3u + c] : pixels[i * 3u + c];
            if (!isfinite(actual) || fabs(actual - expected) > bound) {
                fprintf(stderr, "BJICC pixel=%zu slot=%zu c=%zu "
                    "got=%.12g expected=%.12g bound=%.9g\n", i, slot, c,
                    actual, expected, bound);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}

int
builtin_jpeg_ink_values(int fixture, int engine, int mode)
{
    static char const *const names[] = {
        "plain8", "adobe8", "ycck8", "plain12", "adobe12", "ycck12",
        "plain16", "adobe16", "ycck16", "adobe8-progressive"
    };
    static char const *const suffix[] = {
        "", "", "-absent", "-rejected", "-failure", "-mismatch",
        "", "-absent", "-arithmetic", "-hierarchical"
    };
    unsigned char fixture_bytes[8192];
    size_t size;
    bj_probe_t probe;
    sixel_allocator_t *allocator;
    sixel_loader_t *loader;
    sixel_chunk_t *chunk;
    sixel_cms_engine_t previous;
    SIXELSTATUS status;
    char order[128];
    char path[128];
    int n;

    if (mode != 0 && mode != 8 && mode != 9 && fixture != 1) { return 2; }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    n = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine(previous);
    if (!n) { return 77; }
    snprintf(path, sizeof(path), "/tests/data/color-numeric/"
        "builtin-jpeg-%s%s.jpg", names[fixture], suffix[mode]);
    if (edge_read_fixture(path, fixture_bytes, sizeof(fixture_bytes),
                           &size) != 0) {
        return 1;
    }
    probe.fixture = fixture;
    probe.engine = engine;
    probe.mode = mode >= 8 ? 0 : mode;
    probe.calls = 0;
    loader = NULL;
    chunk = NULL;
    allocator = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_chunk_create_from_memory(&chunk, fixture_bytes, size, NULL,
                                            allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) { goto end; }
    n = snprintf(order, sizeof(order),
        "builtin:cms_engine=%s:cms_intent=relative!!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
    if (n < 0 || (size_t)n >= sizeof(order)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    if (SIXEL_FAILED(status)) { goto end; }
    sixel_loader_set_prefer_float32(loader, mode == 6 || mode == 7);
    status = sixel_loader_load_chunk(loader, chunk, bj_capture);
end:
    sixel_loader_unref(loader);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(previous);
    if (SIXEL_FAILED(status) || probe.calls != 1) {
        fprintf(stderr, "BJICC fixture=%d engine=%d mode=%d status=%x: %s\n",
                fixture, engine, mode, status,
                sixel_helper_get_additional_message());
        return 1;
    }
    return 0;
}
