/*
 * Verify GD pixel-level policy details that pixelformat-only checks miss.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD

#define GD_PROBE_MAX_PIXELS 64

typedef struct gd_pixel_probe {
    int callback_count;
    int pixelformat;
    int colorspace;
    int width;
    int height;
    int transparent;
    int ncolors;
    int alpha_zero_is_transparent;
    int has_transparent_mask;
    size_t transparent_mask_size;
    size_t pixel_count;
    size_t pixel_bytes;
    float pixels_f32[GD_PROBE_MAX_PIXELS * 3u];
    unsigned char pixels_u8[GD_PROBE_MAX_PIXELS * 3u];
    unsigned char indexed_pixels[GD_PROBE_MAX_PIXELS];
    unsigned char transparent_mask[GD_PROBE_MAX_PIXELS];
} gd_pixel_probe_t;

static char const *
loader_test_source_root(void)
{
    char const *source_root;

    source_root = sixel_compat_getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    return source_root;
}

static SIXELSTATUS
capture_gd_pixel_probe(sixel_frame_t *frame, void *data)
{
    gd_pixel_probe_t *probe;
    size_t pixel_count;
    size_t pixel_bytes;
    unsigned char const *pixels_u8;
    float const *pixels_f32;
    unsigned char const *indexed_pixels;
    sixel_frame_pixels_view_t view;
    size_t copy_pixels;
    size_t copy_pixel_bytes;
    size_t copy_mask_bytes;
    SIXELSTATUS status;

    probe = NULL;
    pixel_count = 0u;
    pixel_bytes = 0u;
    pixels_u8 = NULL;
    pixels_f32 = NULL;
    indexed_pixels = NULL;
    memset(&view, 0, sizeof(view));
    copy_pixels = 0u;
    copy_pixel_bytes = 0u;
    copy_mask_bytes = 0u;
    status = SIXEL_FALSE;
    if (frame == NULL || data == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    probe = (gd_pixel_probe_t *)data;
    probe->callback_count += 1;
    probe->pixelformat = sixel_frame_get_pixelformat(frame);
    probe->colorspace = sixel_frame_get_colorspace(frame);
    probe->width = sixel_frame_get_width(frame);
    probe->height = sixel_frame_get_height(frame);
    probe->transparent = sixel_frame_get_transparent(frame);
    probe->ncolors = sixel_frame_get_ncolors(frame);
    probe->alpha_zero_is_transparent = frame->alpha_zero_is_transparent;
    probe->has_transparent_mask = frame->transparent_mask != NULL ? 1 : 0;
    probe->transparent_mask_size = frame->transparent_mask_size;

    if (probe->width <= 0 || probe->height <= 0) {
        return SIXEL_OK;
    }

    if ((size_t)probe->width > SIZE_MAX / (size_t)probe->height) {
        return SIXEL_OK;
    }
    pixel_count = (size_t)probe->width * (size_t)probe->height;
    probe->pixel_count = pixel_count;
    copy_pixels = pixel_count;
    if (copy_pixels > GD_PROBE_MAX_PIXELS) {
        copy_pixels = GD_PROBE_MAX_PIXELS;
    }

    switch (probe->pixelformat) {
    case SIXEL_PIXELFORMAT_PAL8:
        indexed_pixels = sixel_frame_get_pixels(frame);
        if (indexed_pixels != NULL && copy_pixels > 0u) {
            memcpy(probe->indexed_pixels, indexed_pixels, copy_pixels);
        }
        break;
    case SIXEL_PIXELFORMAT_RGB888:
        if (pixel_count > SIZE_MAX / 3u) {
            return SIXEL_OK;
        }
        pixel_bytes = pixel_count * 3u;
        probe->pixel_bytes = pixel_bytes;
        copy_pixel_bytes = copy_pixels * 3u;
        pixels_u8 = sixel_frame_get_pixels(frame);
        if (pixels_u8 != NULL && copy_pixel_bytes > 0u) {
            memcpy(probe->pixels_u8, pixels_u8, copy_pixel_bytes);
        }
        break;
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
        if (pixel_count > SIZE_MAX / 3u) {
            return SIXEL_OK;
        }
        pixel_bytes = pixel_count * 3u;
        probe->pixel_bytes = pixel_bytes;
        copy_pixel_bytes = copy_pixels * 3u;
        status = loader_test_frame_get_pixels_view(frame, &view);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        pixels_f32 = view.pixels_float32;
        if (pixels_f32 != NULL && copy_pixel_bytes > 0u) {
            memcpy(probe->pixels_f32,
                   pixels_f32,
                   copy_pixel_bytes * sizeof(float));
        }
        break;
    default:
        break;
    }

    if (frame->transparent_mask != NULL && copy_pixels > 0u) {
        copy_mask_bytes = copy_pixels;
        if (copy_mask_bytes > frame->transparent_mask_size) {
            copy_mask_bytes = frame->transparent_mask_size;
        }
        memcpy(probe->transparent_mask,
               frame->transparent_mask,
               copy_mask_bytes);
    }

    return SIXEL_OK;
}

static int
run_gd_probe(char const *relative_path,
             int use_palette,
             int reqcolors,
             unsigned char const *bgcolor,
             gd_pixel_probe_t *probe)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback_state;
    char path[PATH_MAX];
    int cancel_flag;
    int require_static;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    cancel_flag = 0;
    require_static = 1;
    memset(&callback_state, 0, sizeof(callback_state));
    if (relative_path == NULL || probe == NULL) {
        return 1;
    }

    if (build_image_path(loader_test_source_root(),
                         relative_path,
                         path,
                         sizeof(path)) != 0) {
        fprintf(stderr, "failed to build path: %s\n", relative_path);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "allocator initialization failed\n");
        return 1;
    }

    status = sixel_chunk_create_from_source(&chunk, path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "failed to read sample: %s\n", relative_path);
        goto error;
    }

    status = create_loader_component_by_name("gd",
                                             allocator,
                                             (void **)&component);
    if (SIXEL_FAILED(status) || component == NULL) {
        fprintf(stderr, "gd component init failed\n");
        goto error;
    }

    memset(probe, 0, sizeof(*probe));
    callback_state.fn = capture_gd_pixel_probe;
    callback_state.context = probe;

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_BGCOLOR,
                                           bgcolor);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "gd load failed for %s (%d)\n",
                relative_path,
                (int)status);
        goto error;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "callback count mismatch for %s (%d)\n",
                relative_path,
                probe->callback_count);
        goto error;
    }

    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return 0;

error:
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return 1;
}

static int
test_partial_alpha_mask_semantics(void)
{
    gd_pixel_probe_t probe;
    size_t i;
    int saw_mask_one;
    int saw_mask_zero_nonblack;

    memset(&probe, 0, sizeof(probe));
    i = 0u;
    saw_mask_one = 0;
    saw_mask_zero_nonblack = 0;
    if (run_gd_probe("/tests/data/inputs/formats/"
                     "libpng-pal8-trns-single0-semi-icc.png",
                     1,
                     SIXEL_PALETTE_MAX,
                     NULL,
                     &probe) != 0) {
        return 1;
    }

    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe.has_transparent_mask != 1 ||
        probe.alpha_zero_is_transparent != 1) {
        fprintf(stderr, "partial alpha policy metadata mismatch\n");
        return 1;
    }

    for (i = 0u; i < probe.pixel_count && i < GD_PROBE_MAX_PIXELS; ++i) {
        if (probe.transparent_mask[i] != 0u) {
            saw_mask_one = 1;
            if (probe.pixels_u8[i * 3u + 0u] != 0u ||
                probe.pixels_u8[i * 3u + 1u] != 0u ||
                probe.pixels_u8[i * 3u + 2u] != 0u) {
                fprintf(stderr, "fully transparent pixel must be blackened\n");
                return 1;
            }
            continue;
        }
        if (probe.pixels_u8[i * 3u + 0u] != 0u ||
            probe.pixels_u8[i * 3u + 1u] != 0u ||
            probe.pixels_u8[i * 3u + 2u] != 0u) {
            saw_mask_zero_nonblack = 1;
        }
    }

    if (saw_mask_one == 0 || saw_mask_zero_nonblack == 0) {
        fprintf(stderr, "partial alpha mask expectations were not observed\n");
        return 1;
    }

    return 0;
}

static int
test_multi_keyonly_pal8_normalized(void)
{
    gd_pixel_probe_t probe;

    memset(&probe, 0, sizeof(probe));
    if (run_gd_probe("/tests/data/inputs/formats/pal8-trns-multi-keyonly.png",
                     1,
                     SIXEL_PALETTE_MAX,
                     NULL,
                     &probe) != 0) {
        return 1;
    }

    if (probe.pixelformat != SIXEL_PIXELFORMAT_PAL8 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe.width != 4 ||
        probe.height != 1 ||
        probe.transparent != 0 ||
        probe.has_transparent_mask != 0 ||
        probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr, "multi-key pal8 policy metadata mismatch\n");
        return 1;
    }

    if (probe.indexed_pixels[0] != 0u ||
        probe.indexed_pixels[1] != 0u ||
        probe.indexed_pixels[2] != 1u ||
        probe.indexed_pixels[3] != 3u) {
        fprintf(stderr, "transparent key normalization mismatch\n");
        return 1;
    }

    return 0;
}

static int
test_gama_only_bg_float32_linear(void)
{
    static unsigned char const white_bg[3] = { 0xffu, 0xffu, 0xffu };
    gd_pixel_probe_t probe;
    float first_r;
    float first_g;
    float first_b;
    float second_r;

    memset(&probe, 0, sizeof(probe));
    first_r = 0.0f;
    first_g = 0.0f;
    first_b = 0.0f;
    second_r = 0.0f;
    if (run_gd_probe("/tests/data/inputs/formats/pal8-trns-key0-gama-only.png",
                     1,
                     SIXEL_PALETTE_MAX,
                     white_bg,
                     &probe) != 0) {
        return 1;
    }

    if (probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace != SIXEL_COLORSPACE_LINEAR ||
        probe.has_transparent_mask != 0 ||
        probe.width != 2 ||
        probe.height != 1) {
        fprintf(stderr, "gAMA-only float32 metadata mismatch\n");
        return 1;
    }

    first_r = probe.pixels_f32[0];
    first_g = probe.pixels_f32[1];
    first_b = probe.pixels_f32[2];
    second_r = probe.pixels_f32[3];
    if (first_r < 0.98f || first_g < 0.98f || first_b < 0.98f) {
        fprintf(stderr, "transparent pixel should composite to white\n");
        return 1;
    }
    if (second_r <= 0.425f) {
        fprintf(stderr, "gAMA transfer branch did not affect red channel\n");
        return 1;
    }

    return 0;
}

static int
test_single_key_nonzero_preserved(void)
{
    gd_pixel_probe_t probe;

    memset(&probe, 0, sizeof(probe));
    if (run_gd_probe("/tests/data/inputs/formats/pal8-trns-key2.png",
                     1,
                     SIXEL_PALETTE_MAX,
                     NULL,
                     &probe) != 0) {
        return 1;
    }

    if (probe.pixelformat != SIXEL_PIXELFORMAT_PAL8 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe.width != 4 ||
        probe.height != 1 ||
        probe.transparent != 2 ||
        probe.has_transparent_mask != 0 ||
        probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr, "single-key nonzero metadata mismatch\n");
        return 1;
    }

    if (probe.indexed_pixels[0] != 0u ||
        probe.indexed_pixels[1] != 2u ||
        probe.indexed_pixels[2] != 1u ||
        probe.indexed_pixels[3] != 3u) {
        fprintf(stderr, "single-key nonzero index mapping mismatch\n");
        return 1;
    }

    return 0;
}

static int
test_partial_alpha_bg_float32_semantics(void)
{
    static unsigned char const white_bg[3] = { 0xffu, 0xffu, 0xffu };
    gd_pixel_probe_t probe;
    size_t i;
    float r;
    float g;
    float b;
    int saw_white;
    int saw_tinted;

    memset(&probe, 0, sizeof(probe));
    i = 0u;
    r = 0.0f;
    g = 0.0f;
    b = 0.0f;
    saw_white = 0;
    saw_tinted = 0;
    if (run_gd_probe("/tests/data/inputs/formats/"
                     "libpng-pal8-trns-single0-semi-icc.png",
                     1,
                     SIXEL_PALETTE_MAX,
                     white_bg,
                     &probe) != 0) {
        return 1;
    }

    if (probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace != SIXEL_COLORSPACE_LINEAR ||
        probe.has_transparent_mask != 0 ||
        probe.width != 6 ||
        probe.height != 1) {
        fprintf(stderr, "partial alpha background float32 metadata mismatch\n");
        return 1;
    }

    for (i = 0u; i < probe.pixel_count && i < GD_PROBE_MAX_PIXELS; ++i) {
        r = probe.pixels_f32[i * 3u + 0u];
        g = probe.pixels_f32[i * 3u + 1u];
        b = probe.pixels_f32[i * 3u + 2u];
        if (r > 0.98f && g > 0.98f && b > 0.98f) {
            saw_white = 1;
            continue;
        }
        if (r > g + 0.05f && r > b + 0.05f && r < 0.98f) {
            saw_tinted = 1;
        }
    }

    if (saw_white == 0 || saw_tinted == 0) {
        fprintf(stderr, "partial alpha background float32 behavior mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_gd_pixelpolicy_mode(char const *mode)
{
    if (mode == NULL || strcmp(mode, "all") == 0) {
        if (test_partial_alpha_mask_semantics() != 0) {
            return 1;
        }
        if (test_multi_keyonly_pal8_normalized() != 0) {
            return 1;
        }
        if (test_gama_only_bg_float32_linear() != 0) {
            return 1;
        }
        if (test_single_key_nonzero_preserved() != 0) {
            return 1;
        }
        if (test_partial_alpha_bg_float32_semantics() != 0) {
            return 1;
        }
        return 0;
    }
    if (strcmp(mode, "partial_alpha_mask_semantics") == 0) {
        return test_partial_alpha_mask_semantics();
    }
    if (strcmp(mode, "multi_keyonly_pal8_normalized") == 0) {
        return test_multi_keyonly_pal8_normalized();
    }
    if (strcmp(mode, "gama_only_bg_float32_linear") == 0) {
        return test_gama_only_bg_float32_linear();
    }
    if (strcmp(mode, "single_key_nonzero_preserved") == 0) {
        return test_single_key_nonzero_preserved();
    }
    if (strcmp(mode, "partial_alpha_bg_float32_semantics") == 0) {
        return test_partial_alpha_bg_float32_semantics();
    }

    fprintf(stderr, "unknown gd pixelpolicy mode: %s\n", mode);
    return 1;
}
#endif

int
test_loader_0059_loader_gd_pixelpolicy_detail(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_GD
    if (argc > 1 && argv != NULL) {
        return run_gd_pixelpolicy_mode(argv[1]);
    }
    return run_gd_pixelpolicy_mode("all");
#else
    fprintf(stderr, "GD loader unavailable\n");
    return SIXEL_TEST_SKIP;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
