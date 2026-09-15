/* SPDX-License-Identifier: MIT */
/* Compare one libpng frame with an original-integer numeric oracle. */
#include "loader_libpng_chrm_test_common.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/cms.h"
#include "src/factory.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-common.h"

#if HAVE_LIBPNG
typedef struct {
    color_png_chrm_t const *source;
    int interpretation;
    int format;
    int calls;
    int matrix_only;
} png_chrm_sample_t;

static SIXELSTATUS
png_chrm_capture(sixel_frame_t *frame, void *context)
{
    png_chrm_sample_t *probe;
    color_png_chrm_t const *source;
    unsigned char const *bytes;
    double expected;
    double actual;
    double tolerance;
    int format;
    int mask;
    int p;
    int c;

    probe = (png_chrm_sample_t *)context;
    source = probe->source;
    ++probe->calls;
    format = probe->format;
    mask = source->alpha && format != SIXEL_PIXELFORMAT_RGBA8888;
    if (frame->width != 8 || frame->height != 1 ||
        frame->pixelformat != format || frame->colorspace !=
            (format == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA) ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->alpha_zero_is_transparent != source->alpha ||
        frame->transparent_mask_size != (mask ? 8u : 0u) ||
        (frame->transparent_mask != NULL) != mask) {
        fprintf(stderr, "PNG cHRM frame representation or coverage differs\n");
        return SIXEL_BAD_INPUT;
    }
    bytes = sixel_frame_get_pixels(frame);
    /* Direct cHRM has its own fixed budget. The optional native ICC control
     * remains separate and is never used to relax a boundary assertion. */
    tolerance = probe->interpretation == 4 &&
        sixel_cms_get_engine() == SIXEL_CMS_ENGINE_COLORSYNC ? 0.000324 : 2e-7;
    for (p = 0; p < 8; ++p) {
        if (mask && frame->transparent_mask[p] !=
            (source->samples[p * 4 + 3] == 0)) {
            return SIXEL_BAD_INPUT;
        }
        if (format == SIXEL_PIXELFORMAT_RGBA8888 &&
            bytes[p * 4 + 3] != source->samples[p * 4 + 3]) {
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            if (format == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
                actual = frame->pixels.f32ptr[p * 3 + c];
                expected = source->expected[probe->interpretation][p * 3 + c];
            } else if (format == SIXEL_PIXELFORMAT_RGBFLOAT32) {
                actual = frame->pixels.f32ptr[p * 3 + c];
                expected = (float)(source->samples[p * 4 + c] / 65535.0);
            } else {
                actual = bytes[p * (format == SIXEL_PIXELFORMAT_RGBA8888
                    ? 4 : 3) + c];
                expected = source->samples[p * 4 + c];
            }
            /* The separate acceptance entry observes the matrix's frame
             * boundary, not accuracy for ill-conditioned valid primaries.
             * Black and finite output still protect successful delivery. */
            if (probe->matrix_only && p != 0) {
                if (!isfinite(actual)) {
                    return SIXEL_BAD_INPUT;
                }
                continue;
            }
            if (!isfinite(actual) || fabs(actual - expected) >
                (format == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                    ? tolerance : 0.0)) {
                fprintf(stderr, "PNG cHRM p=%d c=%d: %a != %a\n",
                    p, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}
#endif

int
test_loader_0198_png_chrm_samples(int argc, char **argv)
{
#if HAVE_LIBPNG
    png_chrm_sample_t probe;
    png_chrm_metadata_t metadata;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous;
    sixel_loader_component_t *component;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    int fixture;
    int one;
    int zero;
    int converted;
    int old_policy;
    int result;

    if ((argc != 3 && (argc != 4 || strcmp(argv[3], "matrix") != 0)) ||
        !sixel_cms_engine_from_string(argv[2], &engine)) {
        return 1;
    }
    fixture = atoi(argv[1]);
    if (fixture < 0 || fixture >=
        (int)(sizeof(color_png_chrm) / sizeof(color_png_chrm[0]))) {
        return 1;
    }
    /* Capability is a build fact, not the result of engine selection. A
     * compiled backend resolving to another engine is a regression. */
#if !HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2) {
        return 77;
    }
#endif
#if !defined(__APPLE__) || !HAVE_COREGRAPHICS
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC) {
        return 77;
    }
#endif
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        fprintf(stderr, "PNG_CHRM_ENGINE|requested=%s|actual=%s\n",
            argv[2], sixel_cms_engine_to_string(sixel_cms_get_engine()));
        sixel_cms_set_engine(previous);
        return 1;
    }
    memset(&probe, 0, sizeof(probe));
    probe.matrix_only = argc == 4;
    probe.source = &color_png_chrm[fixture];
    allocator = NULL;
    component = NULL;
    chunk = NULL;
    one = 1;
    zero = 0;
    result = 1;
    old_policy = loader_transparent_policy();
    sixel_helper_set_loader_transparent_policy(SIXEL_ALPHA_POLICY_CLEAR);
    if (png_chrm_read_metadata(probe.source, &metadata)) {
        goto end;
    }
    if (probe.matrix_only && (engine == SIXEL_CMS_ENGINE_NONE ||
        !metadata.chrm || !probe.source->valid_chrm ||
        probe.source->srgb_priority || probe.source->icc_priority ||
        probe.source->alpha || probe.source->background)) {
        goto end;
    }
    if (engine != SIXEL_CMS_ENGINE_NONE && !probe.source->srgb_priority) {
        probe.interpretation = metadata.gamma ? 1 : 0;
        if (metadata.chrm && probe.source->valid_chrm) {
            probe.interpretation += 2;
        }
        if (metadata.icc && probe.source->icc_priority) {
            probe.interpretation = 4;
        }
    }
    converted = probe.interpretation != 0;
    probe.format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    if (!probe.source->alpha && !converted) {
        probe.format = probe.source->depth == 8 ? SIXEL_PIXELFORMAT_RGB888
                                               : SIXEL_PIXELFORMAT_RGBFLOAT32;
    } else if (probe.source->alpha && probe.source->depth == 8 &&
        !probe.source->background && engine == SIXEL_CMS_ENGINE_NONE) {
        probe.format = SIXEL_PIXELFORMAT_RGBA8888;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_chunk_create_from_memory(&chunk, probe.source->bytes,
        probe.source->size, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    /* Resolve exactly this component through the normal class factory. */
    status = sixel_factory_get_default((void **)&factory);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = factory->vtbl->create(factory, "loader/libpng", allocator,
                                   (void **)&component);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status) ||
        strcmp(sixel_loader_component_get_name(component), "libpng") != 0) {
        goto end;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_REQUIRE_STATIC, &one);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_USE_PALETTE, &zero);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32, &one);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_component_load(component, chunk,
                                          png_chrm_capture, &probe);
    result = status != SIXEL_OK || probe.calls != 1;
    if (!result) {
        fprintf(stderr, "%s|fixture=%d|engine=%s|checked=%d|"
            "interpretation=%d\n", probe.matrix_only ? "PNG_CHRM_ACCEPT"
                : "PNG_CHRM_SAMPLE", fixture, argv[2],
            probe.matrix_only ? 3 : 24, probe.interpretation);
    }
end:
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    sixel_helper_set_loader_transparent_policy(old_policy);
    sixel_cms_set_engine(previous);
    return result;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
