/* SPDX-License-Identifier: MIT */
/* Check source PLTE coordinates against the independent matrix/TRC oracle. */
#include <math.h>
#include <stdint.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/png-samples.h"
#include "cg_indexed_test_common.h"
#include "src/cms.h"
#include "src/loader-common.h"

#if HAVE_COREGRAPHICS
#define CGI_COUNT 15u
#define CGI_FLOAT_BYTES (CGI_COUNT * 3u * sizeof(float))

typedef struct {
    int colors;
    int mode;
    int calls;
} cgi_probe_t;

static int cgi_live;
static int cgi_fail_float;
static int cgi_float_allocations;

static void *
cgi_malloc(size_t size)
{
    void *p;

    if (size == CGI_FLOAT_BYTES) {
        ++cgi_float_allocations;
        if (cgi_fail_float) {
            cgi_fail_float = 0;
            return NULL;
        }
    }
    p = malloc(size);
    if (p != NULL) { ++cgi_live; }
    return p;
}

static void
cgi_free(void *p)
{
    if (p != NULL) { --cgi_live; }
    free(p);
}

static void *
cgi_calloc(size_t n, size_t size)
{
    void *p;

    if (size != 0u && n > SIZE_MAX / size) { return NULL; }
    p = cgi_malloc(n * size);
    if (p != NULL) { memset(p, 0, n * size); }
    return p;
}

static void *
cgi_realloc(void *p, size_t size)
{
    void *q;
    int was_null;

    if (size == 0u) { cgi_free(p); return NULL; }
    was_null = p == NULL;
    q = realloc(p, size);
    if (q != NULL && was_null) { ++cgi_live; }
    return q;
}

static SIXELSTATUS
cgi_capture(sixel_frame_t *frame, void *context)
{
    cgi_probe_t *probe;
    int byte_path;
    int srgb;
    int p;
    int c;
    int slot;
    double expected;
    double actual;

    probe = (cgi_probe_t *)context;
    ++probe->calls;
    byte_path = probe->mode == 3;
    srgb = byte_path || probe->mode == 4;
    if (frame->width != 5 || frame->height != 3 ||
        frame->pixelformat != (byte_path ? SIXEL_PIXELFORMAT_PAL8 :
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (byte_path ? SIXEL_COLORSPACE_GAMMA :
            SIXEL_COLORSPACE_LINEAR) || !frame->owns_pixels ||
        frame->transparent != -1 ||
        frame->ncolors != (byte_path ? probe->colors : -1) ||
        (frame->palette != NULL) != byte_path ||
        frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u ||
        frame->alpha_zero_is_transparent != 0) {
        fprintf(stderr, "CG indexed frame format=%x colors=%d mask=%d\n",
                frame->pixelformat, frame->ncolors,
                frame->transparent_mask != NULL);
        return SIXEL_BAD_INPUT;
    }
    for (p = 0; p < (int)CGI_COUNT; ++p) {
        slot = p % probe->colors;
        if (byte_path && frame->pixels.u8ptr[p] != slot) {
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            if (byte_path) {
                if (frame->palette[slot * 3 + c] !=
                    color_png_source[0][slot][c]) {
                    return SIXEL_BAD_INPUT;
                }
                continue;
            }
            expected = srgb ? color_png_srgb_linear[0][slot][c] :
                              color_png_icc_linear[0][slot][c];
            actual = frame->pixels.f32ptr[p * 3 + c];
            if (!isfinite(actual) || fabs(actual - expected) > 0.0001) {
                fprintf(stderr, "CG indexed pixel=%d channel=%d "
                        "%.12g != %.12g\n", p, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}
#endif

int
cg_indexed_values(char const *fixture, int bits, int mode)
{
#if HAVE_COREGRAPHICS
    unsigned char bytes[4096];
    size_t size;
    cgi_probe_t probe;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    sixel_cms_engine_t previous_engine;
    int palette;
    int colors;

    if (edge_read_fixture(fixture, bytes, sizeof(bytes), &size) != 0) {
        return 1;
    }
    memset(&probe, 0, sizeof(probe));
    probe.colors = bits < 3 ? 1 << bits : 8;
    probe.mode = mode;
    allocator = NULL;
    chunk = NULL;
    loader = NULL;
    palette = mode != 1;
    colors = mode == 2 ? 2 : 256;
    previous_engine = sixel_cms_get_engine();
    sixel_cms_set_engine(mode == 5 ? SIXEL_CMS_ENGINE_NONE :
        mode == 6 ? SIXEL_CMS_ENGINE_BUILTIN : SIXEL_CMS_ENGINE_AUTO);
    cgi_live = 0;
    cgi_fail_float = 0;
    cgi_float_allocations = 0;
    status = sixel_allocator_new(&allocator, cgi_malloc, cgi_calloc,
                                  cgi_realloc, cgi_free);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_chunk_create_from_memory(&chunk, bytes, size,
                                            NULL, allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 "coregraphics!");
    if (SIXEL_FAILED(status)) { goto end; }
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
                               &palette);
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_REQCOLORS, &colors);
    sixel_loader_set_prefer_float32(loader, mode == 4);
    cgi_float_allocations = 0;
    cgi_fail_float = mode == 11;
    status = sixel_loader_load_chunk(loader, chunk, cgi_capture);
    if (mode == 11) {
        if (SIXEL_SUCCEEDED(status) || probe.calls != 0 || cgi_fail_float ||
            cgi_float_allocations != 1) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        /* The same loader can retry after an unpublished allocation failure. */
        status = sixel_loader_load_chunk(loader, chunk, cgi_capture);
    }
    if (SIXEL_SUCCEEDED(status) && (probe.calls != 1 ||
        (mode == 3 && cgi_float_allocations != 0))) {
        status = SIXEL_BAD_INPUT;
    }
end:
    cgi_fail_float = 0;
    sixel_loader_unref(loader);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(previous_engine);
    if (SIXEL_FAILED(status) || cgi_live != 0) {
        fprintf(stderr, "CG indexed mode=%d status=%x calls=%d live=%d: %s\n",
                mode, status, probe.calls, cgi_live,
                sixel_helper_get_additional_message());
        return 1;
    }
    return 0;
#else
    (void)fixture;
    (void)bits;
    (void)mode;
    return 77;
#endif
}
