/*
 * Verify builtin loader reports expected pixelformats:
 * - RGBA(8-bit)  -> RGBA8888 (default alpha keycolor path keeps alpha)
 * - GIF(opaque, palette on) -> PAL8
 * - GIF(alpha, palette on)  -> PAL8
 * - GIF(opaque, palette off) -> RGB888
 * - GIF(alpha, palette off)  -> RGBA8888
 * - GIF(alpha, palette on, low reqcolors) -> RGBA8888 fallback
 * - GIF(alpha, palette on, low reqcolors + bgcolor) -> RGB888 fallback
 * - GIF(anim without NETSCAPE extension) reports multiframe metadata
 * - GIF(loop=auto/force) keeps unbounded NETSCAPE loop behavior
 * - HDR(RGBE) -> LINEARRGBFLOAT32
 * - Gray(16-bit) -> RGBFLOAT32 (no 8-bit precision loss)
 * - PSD RGB16/RGB32 callbacks keep float precision (RGBFLOAT32)
 * - PSD CMYK32/Lab32 callbacks keep float precision family
 * - PSD RGB8+alpha callback stays RGB888 (mask side-channel handles alpha)
 * - PIC RGBA callback keeps alpha-zero mask and applies bgcolor blend
 * - TGA RGBA callback keeps alpha-zero mask and returns RGB888
 * - TGA indexed RGBA callback keeps PAL8 and collapses transparent index
 */

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/cms.h"
#include "src/frompsd.h"
#include "src/loader-common.h"


#include "0014_loader_builtin_pixelformat_common.inc.c"
#include "0014_loader_builtin_pixelformat_hdr.inc.c"
#include "0014_loader_builtin_pixelformat_gif_psd.inc.c"

typedef int (*builtin_loader_env_test_fn_t)(void);

typedef struct builtin_loader_env_dispatch_entry {
    char const *env_name;
    builtin_loader_env_test_fn_t fn;
} builtin_loader_env_dispatch_entry_t;

typedef struct builtin_loader_env_dispatch_group {
    builtin_loader_env_dispatch_entry_t const *entries;
    size_t entry_count;
} builtin_loader_env_dispatch_group_t;

typedef struct pic_rgba_alpha_probe_context {
    int callback_count;
    int pixelformat;
    int width;
    int height;
    int alpha_zero_is_transparent;
    int has_transparent_mask;
    size_t transparent_mask_size;
    unsigned char pixels[16];
    unsigned char transparent_mask[4];
} pic_rgba_alpha_probe_context_t;

typedef struct tga_rgba_alpha_probe_context {
    int callback_count;
    int pixelformat;
    int width;
    int height;
    int transparent;
    int alpha_zero_is_transparent;
    int has_transparent_mask;
    size_t transparent_mask_size;
    unsigned char pixels[12];
    unsigned char transparent_mask[4];
} tga_rgba_alpha_probe_context_t;

typedef struct tga_pal_rgba_probe_context {
    int callback_count;
    int pixelformat;
    int width;
    int height;
    int ncolors;
    int transparent;
    int alpha_zero_is_transparent;
    int has_transparent_mask;
    size_t transparent_mask_size;
    unsigned char pixels[4];
    unsigned char palette[12];
} tga_pal_rgba_probe_context_t;

static SIXELSTATUS
capture_pic_rgba_alpha_probe(sixel_frame_t *frame, void *data)
{
    pic_rgba_alpha_probe_context_t *context;

    context = (pic_rgba_alpha_probe_context_t *)data;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    context->alpha_zero_is_transparent = frame->alpha_zero_is_transparent;
    context->has_transparent_mask = frame->transparent_mask != NULL ? 1 : 0;
    context->transparent_mask_size = frame->transparent_mask_size;

    if (frame->pixels.u8ptr != NULL &&
        context->pixelformat == SIXEL_PIXELFORMAT_RGBA8888 &&
        context->width == 2 &&
        context->height == 2) {
        memcpy(context->pixels, frame->pixels.u8ptr, sizeof(context->pixels));
    }
    if (frame->transparent_mask != NULL && frame->transparent_mask_size >= 4u) {
        memcpy(context->transparent_mask,
               frame->transparent_mask,
               sizeof(context->transparent_mask));
    }

    return SIXEL_OK;
}

static int
run_builtin_loader_pic_rgba_alpha_mask_bgcolor_numeric_test(void)
{
    static unsigned char const bgcolor_white[3] = { 0xffu, 0xffu, 0xffu };
    static unsigned char const expected_first_three_pixels[12] = {
        0xffu, 0x00u, 0x00u, 0xffu,
        0x7eu, 0xfeu, 0x7eu, 0xffu,
        0xbeu, 0xbeu, 0xfeu, 0xffu
    };
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    pic_rgba_alpha_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_white;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader pic rgba alpha mask/bgcolor numeric",
        "/tests/data/inputs/formats/pic_valid_raw_rgba_2x2.pic",
        &options,
        capture_pic_rgba_alpha_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGBA8888) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 1) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "alpha_zero_is_transparent mismatch (%d)\n",
                probe.alpha_zero_is_transparent);
        return 1;
    }
    if (probe.has_transparent_mask != 1 || probe.transparent_mask_size < 4u) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "transparent mask missing (%d, %zu)\n",
                probe.has_transparent_mask,
                probe.transparent_mask_size);
        return 1;
    }
    if (memcmp(probe.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "transparent mask mismatch\n");
        return 1;
    }
    if (memcmp(probe.pixels,
               expected_first_three_pixels,
               sizeof(expected_first_three_pixels)) != 0) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "unexpected semi-alpha composite values\n");
        return 1;
    }
    if (probe.pixels[15] != 0u) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "alpha-zero pixel did not stay transparent\n");
        return 1;
    }

    return 0;
}

static int
verify_tga_rgba_alpha_probe(
    char const *label,
    tga_rgba_alpha_probe_context_t const *probe,
    unsigned char const *expected_pixels,
    unsigned char const *expected_mask)
{
    if (label == NULL ||
        probe == NULL ||
        expected_pixels == NULL ||
        expected_mask == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (probe->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if (probe->width != 2 || probe->height != 2) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (probe->transparent != -1) {
        fprintf(stderr, "%s: transparent index mismatch (%d)\n",
                label,
                probe->transparent);
        return 1;
    }
    if (probe->alpha_zero_is_transparent != 1) {
        fprintf(stderr, "%s: alpha_zero_is_transparent mismatch (%d)\n",
                label,
                probe->alpha_zero_is_transparent);
        return 1;
    }
    if (probe->has_transparent_mask != 1 || probe->transparent_mask_size < 4u) {
        fprintf(stderr, "%s: transparent mask missing (%d, %zu)\n",
                label,
                probe->has_transparent_mask,
                probe->transparent_mask_size);
        return 1;
    }
    if (memcmp(probe->transparent_mask, expected_mask, 4u) != 0) {
        fprintf(stderr, "%s: transparent mask mismatch\n", label);
        return 1;
    }
    if (memcmp(probe->pixels, expected_pixels, 12u) != 0) {
        fprintf(stderr, "%s: composite RGB mismatch\n", label);
        return 1;
    }

    return 0;
}

static SIXELSTATUS
capture_tga_rgba_alpha_probe(sixel_frame_t *frame, void *data)
{
    tga_rgba_alpha_probe_context_t *context;

    context = (tga_rgba_alpha_probe_context_t *)data;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    context->transparent = sixel_frame_get_transparent(frame);
    context->alpha_zero_is_transparent = frame->alpha_zero_is_transparent;
    context->has_transparent_mask = frame->transparent_mask != NULL ? 1 : 0;
    context->transparent_mask_size = frame->transparent_mask_size;
    if (frame->pixels.u8ptr != NULL &&
        context->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
        context->width == 2 &&
        context->height == 2) {
        memcpy(context->pixels, frame->pixels.u8ptr, sizeof(context->pixels));
    }
    if (frame->transparent_mask != NULL && frame->transparent_mask_size >= 4u) {
        memcpy(context->transparent_mask,
               frame->transparent_mask,
               sizeof(context->transparent_mask));
    }

    return SIXEL_OK;
}

static SIXELSTATUS
capture_tga_pal_rgba_probe(sixel_frame_t *frame, void *data)
{
    tga_pal_rgba_probe_context_t *context;

    context = (tga_pal_rgba_probe_context_t *)data;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    context->ncolors = sixel_frame_get_ncolors(frame);
    context->transparent = sixel_frame_get_transparent(frame);
    context->alpha_zero_is_transparent = frame->alpha_zero_is_transparent;
    context->has_transparent_mask = frame->transparent_mask != NULL ? 1 : 0;
    context->transparent_mask_size = frame->transparent_mask_size;
    if (frame->pixels.u8ptr != NULL &&
        context->pixelformat == SIXEL_PIXELFORMAT_PAL8 &&
        context->width == 2 &&
        context->height == 2) {
        memcpy(context->pixels, frame->pixels.u8ptr, sizeof(context->pixels));
    }
    if (frame->palette != NULL && context->ncolors >= 4) {
        memcpy(context->palette, frame->palette, sizeof(context->palette));
    }

    return SIXEL_OK;
}

static int
run_builtin_loader_tga_rgba_alpha_mask_bgcolor_numeric_test(void)
{
    static unsigned char const bgcolor_white[3] = { 0xffu, 0xffu, 0xffu };
    static unsigned char const expected_black_rgb[12] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x7fu, 0x00u,
        0x00u, 0x00u, 0x3fu,
        0x00u, 0x00u, 0x00u
    };
    static unsigned char const expected_white_rgb[12] = {
        0xffu, 0x00u, 0x00u,
        0x7eu, 0xfeu, 0x7eu,
        0xbeu, 0xbeu, 0xfeu,
        0xfeu, 0xfeu, 0xfeu
    };
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    tga_rgba_alpha_probe_context_t probe_black;
    tga_rgba_alpha_probe_context_t probe_white;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    memset(&options, 0, sizeof(options));
    memset(&probe_black, 0, sizeof(probe_black));
    memset(&probe_white, 0, sizeof(probe_white));

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader tga rgba alpha mask/bgcolor numeric (default black)",
        "/tests/data/inputs/formats/tga-rgba-2x2-top-left.tga",
        &options,
        capture_tga_rgba_alpha_probe,
        &probe_black,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader tga rgba alpha mask/bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    result = verify_tga_rgba_alpha_probe(
        "builtin loader tga rgba alpha mask/bgcolor numeric (default black)",
        &probe_black,
        expected_black_rgb,
        expected_mask);
    if (result != 0) {
        return result;
    }

    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_white;
    result = run_builtin_loader_probe_case(
        "builtin loader tga rgba alpha mask/bgcolor numeric (white bgcolor)",
        "/tests/data/inputs/formats/tga-rgba-2x2-top-left.tga",
        &options,
        capture_tga_rgba_alpha_probe,
        &probe_white,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader tga rgba alpha mask/bgcolor numeric: "
                "loader failed with bgcolor (%d)\n",
                (int)status);
        return 1;
    }
    result = verify_tga_rgba_alpha_probe(
        "builtin loader tga rgba alpha mask/bgcolor numeric (white bgcolor)",
        &probe_white,
        expected_white_rgb,
        expected_mask);
    if (result != 0) {
        return result;
    }
    if (memcmp(probe_black.pixels, probe_white.pixels, 12u) == 0) {
        fprintf(stderr,
                "builtin loader tga rgba alpha mask/bgcolor numeric: "
                "bgcolor did not affect composite RGB\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_tga_pal_rgba_transparent_index_numeric_test(void)
{
    static unsigned char const expected_pixels[4] = {
        0u, 1u, 1u, 3u
    };
    static unsigned char const expected_palette[12] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0xffu, 0xffu, 0x00u
    };
    builtin_loader_probe_options_t options;
    tga_pal_rgba_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));

    options.require_static = 1;
    options.use_palette = 1;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader tga indexed rgba transparent index numeric",
        "/tests/data/inputs/formats/tga-pal-rgba-2x2-top-left.tga",
        &options,
        capture_tga_pal_rgba_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_PAL8) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.ncolors != 4) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "ncolors mismatch (%d)\n",
                probe.ncolors);
        return 1;
    }
    if (probe.transparent != 1) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "transparent index mismatch (%d)\n",
                probe.transparent);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "alpha_zero_is_transparent mismatch (%d)\n",
                probe.alpha_zero_is_transparent);
        return 1;
    }
    if (probe.has_transparent_mask != 0 || probe.transparent_mask_size != 0u) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "unexpected transparent mask (%d, %zu)\n",
                probe.has_transparent_mask,
                probe.transparent_mask_size);
        return 1;
    }
    if (memcmp(probe.pixels, expected_pixels, sizeof(expected_pixels)) != 0) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "collapsed palette index mismatch\n");
        return 1;
    }
    if (memcmp(probe.palette,
               expected_palette,
               sizeof(expected_palette)) != 0) {
        fprintf(stderr,
                "builtin loader tga indexed rgba transparent index numeric: "
                "palette composite mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_env_dispatch(
    builtin_loader_env_dispatch_entry_t const *entries,
    size_t entry_count)
{
    size_t index;
    char const *mode;

    index = 0u;
    mode = NULL;
    if (entries == NULL) {
        return -1;
    }

    for (index = 0u; index < entry_count; ++index) {
        mode = loader_test_getenv(entries[index].env_name);
        if (mode != NULL && strcmp(mode, "1") == 0) {
            return entries[index].fn();
        }
    }
    return -1;
}

static int
run_builtin_loader_env_dispatch_groups(
    builtin_loader_env_dispatch_group_t const *groups,
    size_t group_count)
{
    size_t group_index;
    int dispatch_result;

    group_index = 0u;
    dispatch_result = -1;
    if (groups == NULL) {
        return -1;
    }

    for (group_index = 0u; group_index < group_count; ++group_index) {
        dispatch_result = run_builtin_loader_env_dispatch(
            groups[group_index].entries,
            groups[group_index].entry_count);
        if (dispatch_result >= 0) {
            return dispatch_result;
        }
    }
    return -1;
}

static int
run_builtin_loader_test(void)
{
    static builtin_loader_env_dispatch_entry_t const hdr_env_dispatch[] = {
        { "SIXEL_TEST_HDR_NUMERIC_GAMMA",
          run_builtin_loader_hdr_gamma_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_FALLBACK_PROFILE",
          run_builtin_loader_hdr_fallback_profile_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_PRIORITY",
          run_builtin_loader_hdr_header_priority_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_EXPOSURE",
          run_builtin_loader_hdr_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_TONEMAP_REINHARD",
          run_builtin_loader_hdr_tonemap_reinhard_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_SINGLE_CASE",
          run_builtin_loader_hdr_single_case_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_FALLBACK",
          run_builtin_loader_hdr_invalid_fallback_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_TONEMAP",
          run_builtin_loader_hdr_invalid_tonemap_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_EXPOSURE",
          run_builtin_loader_hdr_invalid_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_EXPOSURE_OVERFLOW_NONE",
          run_builtin_loader_hdr_exposure_overflow_none_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_EXPOSURE_OVERFLOW_REINHARD",
          run_builtin_loader_hdr_exposure_overflow_reinhard_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_EXPOSURE",
          run_builtin_loader_hdr_header_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_EXPOSURE_MULTI",
          run_builtin_loader_hdr_header_exposure_multi_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_EXPOSURE_DISABLED",
          run_builtin_loader_hdr_header_exposure_disabled_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_XYZE",
          run_builtin_loader_hdr_xyze_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_COLORCORR_SINGLE",
          run_builtin_loader_hdr_colorcorr_single_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_COLORCORR_MULTI",
          run_builtin_loader_hdr_colorcorr_multi_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_ORIENTATION_ALL",
          run_builtin_loader_hdr_orientation_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_GAMMA_INVALID_PRIMARIES_VALID",
          run_builtin_loader_hdr_gamma_invalid_primaries_valid_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_PRIMARIES_INVALID_GAMMA_VALID",
          run_builtin_loader_hdr_primaries_invalid_gamma_valid_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_HEADER_EXPOSURE",
          run_builtin_loader_hdr_invalid_header_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_MIXED_HEADER_EXPOSURE_INVALID",
          run_builtin_loader_hdr_mixed_header_exposure_invalid_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_USE_HEADER_EXPOSURE_ENV",
          run_builtin_loader_hdr_invalid_use_hdr_exposure_env_test },
        { "SIXEL_TEST_HDR_NUMERIC_DUPLICATE_HEADER_METADATA_LAST_WINS",
          run_builtin_loader_hdr_duplicate_header_metadata_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_PIXASPECT_VIEW_METADATA",
          run_builtin_loader_hdr_pixaspect_view_metadata_numeric_test }
    };
    static builtin_loader_env_dispatch_entry_t const gif_env_dispatch[] = {
        { "SIXEL_TEST_GIF_LOOP_DISABLE_LOOP0_ONCE",
          run_builtin_loader_gif_loop_disable_loop0_once_test },
        { "SIXEL_TEST_GIF_LOOP_DISABLE_LOOP1_ONCE",
          run_builtin_loader_gif_loop_disable_loop1_once_test },
        { "SIXEL_TEST_GIF_LOOP_DISABLE_LOOP2_ONCE",
          run_builtin_loader_gif_loop_disable_loop2_once_test },
        { "SIXEL_TEST_GIF_LOOP_AUTO_LOOP1_ONCE",
          run_builtin_loader_gif_loop_auto_loop1_once_test },
        { "SIXEL_TEST_GIF_LOOP_AUTO_LOOP2_TWICE",
          run_builtin_loader_gif_loop_auto_loop2_twice_test },
        { "SIXEL_TEST_GIF_LOOP_AUTO_LOOP0_UNBOUNDED",
          run_builtin_loader_gif_loop_auto_loop0_unbounded_test },
        { "SIXEL_TEST_GIF_LOOP_FORCE_LOOP0_UNBOUNDED",
          run_builtin_loader_gif_loop_force_loop0_unbounded_test },
        { "SIXEL_TEST_GIF_LOOP_FORCE_LOOP1_UNBOUNDED",
          run_builtin_loader_gif_loop_force_loop1_unbounded_test },
        { "SIXEL_TEST_GIF_LOOP_FORCE_LOOP2_UNBOUNDED",
          run_builtin_loader_gif_loop_force_loop2_unbounded_test }
    };
    static builtin_loader_env_dispatch_entry_t const tga_env_dispatch[] = {
        { "SIXEL_TEST_TGA_NUMERIC_RGBA_ALPHA_MASK_BGCOLOR",
          run_builtin_loader_tga_rgba_alpha_mask_bgcolor_numeric_test },
        { "SIXEL_TEST_TGA_NUMERIC_PAL_RGBA_TRANSPARENT_INDEX",
          run_builtin_loader_tga_pal_rgba_transparent_index_numeric_test }
    };
    static builtin_loader_env_dispatch_entry_t const psd_env_dispatch[] = {
        { "SIXEL_TEST_PSD_VALIDATE_DEFENSIVE",
          run_builtin_loader_psd_validate_defensive_test },
        { "SIXEL_TEST_PIC_NUMERIC_RGBA_ALPHA_MASK_BGCOLOR",
          run_builtin_loader_pic_rgba_alpha_mask_bgcolor_numeric_test }
    };
    static builtin_loader_env_dispatch_group_t const env_dispatch_groups[] = {
        {
            hdr_env_dispatch,
            sizeof(hdr_env_dispatch) / sizeof(hdr_env_dispatch[0])
        },
        {
            gif_env_dispatch,
            sizeof(gif_env_dispatch) / sizeof(gif_env_dispatch[0])
        },
        {
            tga_env_dispatch,
            sizeof(tga_env_dispatch) / sizeof(tga_env_dispatch[0])
        },
        {
            psd_env_dispatch,
            sizeof(psd_env_dispatch) / sizeof(psd_env_dispatch[0])
        }
    };
    char const *expected_cms_pixelformat_text;
    unsigned char const bgcolor_white[3] = { 0xffu, 0xffu, 0xffu };
    int cms_target_pixelformat;
    int cms_target_colorspace;
    int parsed_pixelformat;
    int dispatch_result;
    int result;
    loader_component_case_spec_t psd_alpha_case;

    dispatch_result = run_builtin_loader_env_dispatch_groups(
        env_dispatch_groups,
        sizeof(env_dispatch_groups) / sizeof(env_dispatch_groups[0]));
    if (dispatch_result >= 0) {
        return dispatch_result;
    }

    result = run_loader_component_case("builtin loader rgba8",
                                       RGBA_IMAGE_PATH,
                                       SIXEL_PIXELFORMAT_RGBA8888,
                                       2,
                                       1,
                                       new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif opaque pal8",
        "/tests/data/inputs/small.gif",
        SIXEL_PIXELFORMAT_PAL8,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        1,
        -1,
        1,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent pal8",
        "/tests/data/inputs/formats/gif-transparent-static.gif",
        SIXEL_PIXELFORMAT_PAL8,
        8,
        8,
        1,
        FRAME_TRANSPARENT_NONNEG,
        0,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif opaque rgb",
        "/tests/data/inputs/small.gif",
        SIXEL_PIXELFORMAT_RGB888,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        1,
        -1,
        1,
        1,
        0,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent rgba",
        "/tests/data/inputs/formats/gif-transparent-static.gif",
        SIXEL_PIXELFORMAT_RGBA8888,
        8,
        8,
        1,
        -1,
        0,
        1,
        0,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent low-reqcolors rgba fallback",
        "/tests/data/inputs/formats/gif-transparent-static-3colors.gif",
        SIXEL_PIXELFORMAT_RGBA8888,
        8,
        8,
        1,
        -1,
        0,
        1,
        1,
        3,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent low-reqcolors rgb fallback with bgcolor",
        "/tests/data/inputs/formats/gif-transparent-static-3colors.gif",
        SIXEL_PIXELFORMAT_RGB888,
        8,
        8,
        1,
        -1,
        0,
        1,
        1,
        3,
        bgcolor_white,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif no-netscape multiframe metadata",
        "/tests/data/inputs/formats/gif-anim-no-netscape-2frame.gif",
        SIXEL_PIXELFORMAT_PAL8,
        6,
        6,
        2,
        -1,
        1,
        0,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif bgindex-oob fallback pal8",
        "/tests/data/inputs/formats/gif-bgindex-oob-anim.gif",
        SIXEL_PIXELFORMAT_PAL8,
        2,
        1,
        1,
        -1,
        1,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif netscape unknown subtype metadata",
        "/tests/data/inputs/formats/gif-anim-netscape-unknown-subtype.gif",
        SIXEL_PIXELFORMAT_PAL8,
        2,
        1,
        2,
        -1,
        1,
        0,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent-index-oob treated opaque",
        "/tests/data/inputs/formats/gif-transparent-index-oob-static.gif",
        SIXEL_PIXELFORMAT_PAL8,
        8,
        8,
        1,
        -1,
        0,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_hdr_case_with_cms("builtin loader hdr cms=off",
                                                  SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                                  SIXEL_COLORSPACE_LINEAR,
                                                  SIXEL_CMS_ENGINE_NONE);
    if (result != 0) {
        return result;
    }

    expected_cms_pixelformat_text = loader_test_getenv(
        "SIXEL_TEST_EXPECT_HDR_CMS_PIXELFORMAT");
    if (expected_cms_pixelformat_text != NULL &&
        expected_cms_pixelformat_text[0] != '\0') {
        if (parse_expected_hdr_cms_pixelformat(expected_cms_pixelformat_text,
                                               &parsed_pixelformat) != 0) {
            fprintf(stderr,
                    "builtin loader hdr cms=on: unknown expected pixelformat "
                    "'%s'\n",
                    expected_cms_pixelformat_text);
            return 1;
        }
        cms_target_pixelformat = parsed_pixelformat;
    } else {
        cms_target_pixelformat = loader_cms_target_pixelformat();
    }
    cms_target_colorspace = expected_colorspace_for_pixelformat(
        cms_target_pixelformat);
    result = run_builtin_loader_hdr_case_with_cms("builtin loader hdr cms=on",
                                                  cms_target_pixelformat,
                                                  cms_target_colorspace,
                                                  SIXEL_CMS_ENGINE_AUTO);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader gray16",
        "/tests/data/inputs/formats/"
        "snake-png-gray16.png",
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd rgb16 keeps float32",
        "/tests/data/inputs/formats/snake16_rgb16_raw.psd",
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd rgb32 keeps float32",
        "/tests/data/inputs/formats/snake16_rgb32_raw.psd",
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd cmyk32 keeps linear float32",
        "/tests/data/inputs/formats/snake16_cmyk32_raw.psd",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd lab32 keeps cielab float32",
        "/tests/data/inputs/formats/snake16_lab32_raw.psd",
        SIXEL_PIXELFORMAT_CIELABFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    psd_alpha_case.label =
        "builtin loader psd rgb8 alpha no-bgcolor keeps rgb888";
    psd_alpha_case.relative_path =
        "/tests/data/inputs/formats/snake16_rgb8_alpha.psd";
    psd_alpha_case.expect.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    psd_alpha_case.expect.width = GEOMETRY_ANY;
    psd_alpha_case.expect.height = GEOMETRY_ANY;
    psd_alpha_case.expect.callback_count = 1;
    psd_alpha_case.expect.transparent = -1;
    psd_alpha_case.expect.multiframe = 0;
    psd_alpha_case.expect.mask_present = 1;
    psd_alpha_case.expect.alpha_zero_is_transparent = 1;
    psd_alpha_case.options.require_static = 1;
    psd_alpha_case.options.use_palette = 0;
    psd_alpha_case.options.reqcolors = 256;
    psd_alpha_case.options.bgcolor = NULL;
    psd_alpha_case.new_component = new_builtin_component_for_pixelformat_test;
    result = run_loader_component_case_from_spec(&psd_alpha_case);
    if (result != 0) {
        return result;
    }

    psd_alpha_case.label =
        "builtin loader psd rgb8 alpha with-bgcolor keeps rgb888";
    psd_alpha_case.expect.mask_present = 0;
    psd_alpha_case.expect.alpha_zero_is_transparent = 0;
    psd_alpha_case.options.bgcolor = bgcolor_white;
    result = run_loader_component_case_from_spec(&psd_alpha_case);
    if (result != 0) {
        return result;
    }

    return 0;
}

int
test_loader_0014_loader_builtin_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    return run_builtin_loader_test();
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
