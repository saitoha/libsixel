/*
 * Verify builtin loader reports expected pixelformats:
 * - RGBA(8-bit)  -> RGB888 + alpha-zero transparent mask
 * - GIF(opaque, palette on) -> PAL8
 * - GIF(alpha, palette on)  -> PAL8
 * - GIF(opaque, palette off) -> RGB888
 * - GIF(alpha, palette off)  -> RGB888 + alpha-zero transparent mask
 * - GIF(alpha, palette on, low reqcolors) -> RGB888 fallback + mask
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/cms.h"
#include "src/frombmp.h"
#include "src/frompsd.h"
#include "src/loader-common.h"

/*
 * pcc warns on long internal test helper identifiers.
 * Keep descriptive names by composing identifiers from prefix/suffix pairs.
 */
#define SIXEL_T0014_CAT2_I(a, b) a##b
#define SIXEL_T0014_CAT2(a, b) SIXEL_T0014_CAT2_I(a, b)
#define SIXEL_T0014_FN(prefix, suffix) SIXEL_T0014_CAT2(prefix, suffix)

#if defined(__PCC__)
# define SIXEL_T0014_PREF_HDR_GINV_PVALID_NUM pcc_t0014_hdr_ginv_pvalid_num
# define SIXEL_T0014_PREF_HDR_PINV_GVALID_NUM pcc_t0014_hdr_pinv_gvalid_num
# define SIXEL_T0014_PREF_HDR_MIX_HDR_EXP_INV_NUM pcc_t0014_hdr_mix_hexp_inv
# define SIXEL_T0014_PREF_PNM_PAM_ULK_STRICT_REG pcc_t0014_pnm_pam_ulk_reg
# define SIXEL_T0014_PREF_PNM_PAM_UT_FALLBACK_BND pcc_t0014_pnm_pam_ut_bnd
#else
# define SIXEL_T0014_PREF_HDR_GINV_PVALID_NUM \
    run_builtin_loader_hdr_gamma_invalid_primaries_valid_numeric
# define SIXEL_T0014_PREF_HDR_PINV_GVALID_NUM \
    run_builtin_loader_hdr_primaries_invalid_gamma_valid_numeric
# define SIXEL_T0014_PREF_HDR_MIX_HDR_EXP_INV_NUM \
    run_builtin_loader_hdr_mixed_header_exposure_invalid_numeric
# define SIXEL_T0014_PREF_PNM_PAM_ULK_STRICT_REG \
    run_builtin_loader_pnm_pam_unknown_long_key_strict_regression
# define SIXEL_T0014_PREF_PNM_PAM_UT_FALLBACK_BND \
    run_builtin_loader_pnm_pam_unknown_tupletype_fallback_boundary
#endif


#include "tests/loader/builtin/0014_loader_builtin_pixelformat_common.inc.c"
#include "tests/loader/builtin/0014_loader_builtin_pixelformat_hdr.inc.c"
#include "tests/loader/builtin/0014_loader_builtin_pixelformat_gif_psd.inc.c"

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
    unsigned char pixels[12];
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

typedef struct pnm_numeric_probe_context {
    int callback_count;
    int pixelformat;
    int colorspace;
    int width;
    int height;
    float pixels_f32[12];
    unsigned char pixels_u8[12];
} pnm_numeric_probe_context_t;

typedef struct bmp_numeric_probe_context {
    int callback_count;
    int pixelformat;
    int colorspace;
    int width;
    int height;
    int transparent;
    int alpha_zero_is_transparent;
    int has_transparent_mask;
    size_t transparent_mask_size;
    float pixels_f32[12];
    unsigned char pixels_u8[12];
    unsigned char transparent_mask[4];
} bmp_numeric_probe_context_t;

static int
run_builtin_loader_probe_buffer_case(char const *label,
                                     unsigned char const *buffer,
                                     size_t buffer_size,
                                     builtin_loader_probe_options_t const
                                         *options,
                                     sixel_load_image_function callback,
                                     void *callback_context,
                                     SIXELSTATUS *load_status_out);

static int
verify_bmp_float_probe_metadata(char const *label,
                                bmp_numeric_probe_context_t const *probe,
                                int expected_width,
                                int expected_height,
                                int expect_mask);

static int
run_builtin_loader_bmp_expect_fail_case(char const *label,
                                        char const *fixture_path);

static int
bmp_num_mk_bi_png40(unsigned char *bmp,
                    size_t bmp_capacity,
                    unsigned int bpp,
                    unsigned char const *payload,
                    size_t payload_size,
                    size_t *bmp_size);

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

static int
run_builtin_loader_pic_rgba_alpha_mask_bgcolor_numeric_test(void)
{
    static unsigned char const bgcolor_white[3] = { 0xffu, 0xffu, 0xffu };
    static unsigned char const expected_pixels[12] = {
        0xffu, 0x00u, 0x00u,
        0xbbu, 0xffu, 0xbbu,
        0xe0u, 0xe0u, 0xffu,
        0xffu, 0xffu, 0xffu
    };
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    pic_rgba_alpha_probe_context_t probe;
    SIXELSTATUS status;
    int result;
    size_t index;

    status = SIXEL_FALSE;
    result = 1;
    index = 0u;
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
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
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
               expected_pixels,
               sizeof(expected_pixels)) != 0) {
        fprintf(stderr,
                "builtin loader pic rgba alpha mask/bgcolor numeric: "
                "unexpected semi-alpha composite values\n");
        fprintf(stderr, "actual:");
        for (index = 0u; index < sizeof(probe.pixels); ++index) {
            fprintf(stderr, " %02x", probe.pixels[index]);
        }
        fprintf(stderr, "\nexpected:");
        for (index = 0u; index < sizeof(expected_pixels); ++index) {
            fprintf(stderr, " %02x", expected_pixels[index]);
        }
        fprintf(stderr, "\n");
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
        0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu,
        0xffu, 0xffu, 0xffu
    };
    static unsigned char const expected_white_rgb[12] = {
        0xffu, 0x00u, 0x00u,
        0xbbu, 0xffu, 0xbbu,
        0xe0u, 0xe0u, 0xffu,
        0xffu, 0xffu, 0xffu
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

#include "tests/loader/builtin/0014_loader_builtin_pixelformat_bmp.inc.c"

static SIXELSTATUS
capture_pnm_numeric_probe(sixel_frame_t *frame, void *data)
{
    pnm_numeric_probe_context_t *context;
    size_t sample_count;
    size_t sample_limit;

    context = (pnm_numeric_probe_context_t *)data;
    sample_count = 0u;
    sample_limit = 0u;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->colorspace = sixel_frame_get_colorspace(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    memset(context->pixels_f32, 0, sizeof(context->pixels_f32));
    memset(context->pixels_u8, 0, sizeof(context->pixels_u8));

    if (context->width <= 0 || context->height <= 0) {
        return SIXEL_OK;
    }
    sample_count = (size_t)context->width * (size_t)context->height * 3u;

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(context->pixelformat)) {
        if (frame->pixels.f32ptr == NULL) {
            return SIXEL_OK;
        }
        sample_limit = sizeof(context->pixels_f32) /
            sizeof(context->pixels_f32[0]);
        if (sample_count > sample_limit) {
            sample_count = sample_limit;
        }
        memcpy(context->pixels_f32,
               frame->pixels.f32ptr,
               sample_count * sizeof(context->pixels_f32[0]));
        return SIXEL_OK;
    }

    if (frame->pixels.u8ptr == NULL) {
        return SIXEL_OK;
    }
    sample_limit = sizeof(context->pixels_u8) /
        sizeof(context->pixels_u8[0]);
    if (sample_count > sample_limit) {
        sample_count = sample_limit;
    }
    memcpy(context->pixels_u8, frame->pixels.u8ptr, sample_count);

    return SIXEL_OK;
}

static int
run_builtin_loader_probe_buffer_case(char const *label,
                                     unsigned char const *buffer,
                                     size_t buffer_size,
                                     builtin_loader_probe_options_t const
                                         *options,
                                     sixel_load_image_function callback,
                                     void *callback_context,
                                     SIXELSTATUS *load_status_out)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback_state;
    sixel_chunk_t chunk;
    int require_static;
    int use_palette;
    int reqcolors;
    int set_bgcolor;
    unsigned char const *bgcolor;
    int loop_control;
    int cms_engine;
    char const *info40_mode_text;
    int info40_mode;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    component = NULL;
    memset(&chunk, 0, sizeof(chunk));
    require_static = 0;
    use_palette = 0;
    reqcolors = 256;
    set_bgcolor = 0;
    bgcolor = NULL;
    loop_control = SIXEL_LOOP_AUTO;
    cms_engine = SIXEL_CMS_ENGINE_NONE;
    info40_mode_text = NULL;
    info40_mode = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO;
    result = 1;
    if (load_status_out != NULL) {
        *load_status_out = SIXEL_FALSE;
    }

    if (label == NULL ||
        buffer == NULL ||
        buffer_size == 0u ||
        options == NULL ||
        callback == NULL) {
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }

    status = new_builtin_component_for_pixelformat_test(allocator, &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    chunk.buffer = (unsigned char *)buffer;
    chunk.size = buffer_size;
    chunk.max_size = buffer_size;
    chunk.allocator = allocator;

    require_static = options->require_static;
    use_palette = options->use_palette;
    reqcolors = options->reqcolors;
    set_bgcolor = options->set_bgcolor;
    bgcolor = options->bgcolor;
    loop_control = options->loop_control;
    cms_engine = options->cms_engine;

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (set_bgcolor != 0) {
        status = sixel_loader_component_setopt(component,
                                               SIXEL_LOADER_OPTION_BGCOLOR,
                                               bgcolor);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (options->set_loop_control != 0) {
        status = sixel_loader_component_setopt(component,
                                               SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                               &loop_control);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (options->set_cms_engine != 0) {
        status = sixel_loader_component_setopt(
            component,
            SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
            &cms_engine);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    info40_mode_text =
        loader_test_getenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE");
    if (info40_mode_text != NULL && info40_mode_text[0] != '\0') {
        if (strcmp(info40_mode_text, "auto") == 0) {
            info40_mode = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO;
        } else if (strcmp(info40_mode_text, "windows") == 0) {
            info40_mode = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS;
        } else if (strcmp(info40_mode_text, "os2") == 0) {
            info40_mode = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2;
        } else {
            fprintf(stderr,
                    "%s: invalid info40 mode env '%s'\n",
                    label,
                    info40_mode_text);
            goto cleanup;
        }
        status = sixel_loader_component_setopt(
            component,
            SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_BMP_INFO40_MODE,
            &info40_mode);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    callback_state.loader = NULL;
    callback_state.fn = callback;
    callback_state.context = callback_context;
    status = sixel_loader_component_load(component,
                                         &chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (load_status_out != NULL) {
        *load_status_out = status;
    }
    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    sixel_allocator_unref(allocator);
    return result;
}

static float
pnm_numeric_decode_srgb_unit(float gamma_value)
{
    if (!(gamma_value > 0.0f)) {
        return 0.0f;
    }
    if (gamma_value >= 1.0f) {
        return 1.0f;
    }
    if (gamma_value <= 0.04045f) {
        return gamma_value / 12.92f;
    }
    return powf((gamma_value + 0.055f) / 1.055f, 2.4f);
}

static void
pnm_numeric_compose_expected_linear(float out_rgb[3],
                                    unsigned char const src_rgb[3],
                                    unsigned char src_alpha,
                                    float const bg_linear[3])
{
    int channel;
    float alpha_unit;
    float inv_alpha;
    float src_gamma;
    float src_linear;

    channel = 0;
    alpha_unit = 0.0f;
    inv_alpha = 0.0f;
    src_gamma = 0.0f;
    src_linear = 0.0f;
    if (out_rgb == NULL || src_rgb == NULL || bg_linear == NULL) {
        return;
    }

    alpha_unit = (float)src_alpha / 255.0f;
    inv_alpha = 1.0f - alpha_unit;
    for (channel = 0; channel < 3; ++channel) {
        src_gamma = (float)src_rgb[channel] / 255.0f;
        src_linear = pnm_numeric_decode_srgb_unit(src_gamma);
        out_rgb[channel] = src_linear * alpha_unit
            + bg_linear[channel] * inv_alpha;
    }
}

static int
verify_pnm_float_probe(char const *label,
                       pnm_numeric_probe_context_t const *probe,
                       int expected_pixelformat,
                       int expected_colorspace,
                       int expected_width,
                       int expected_height,
                       float const expected_rgb[3],
                       float tolerance)
{
    int channel;

    channel = 0;
    if (label == NULL || probe == NULL || expected_rgb == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (probe->pixelformat != expected_pixelformat) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if (probe->colorspace != expected_colorspace) {
        fprintf(stderr, "%s: colorspace mismatch (%d)\n",
                label,
                probe->colorspace);
        return 1;
    }
    if (probe->width != expected_width || probe->height != expected_height) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }

    for (channel = 0; channel < 3; ++channel) {
        if (!float_approx_equal(probe->pixels_f32[channel],
                                expected_rgb[channel],
                                tolerance)) {
            fprintf(stderr,
                    "%s: channel %d mismatch (actual=%0.8f expected=%0.8f)\n",
                    label,
                    channel,
                    probe->pixels_f32[channel],
                    expected_rgb[channel]);
            return 1;
        }
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_rgba_linear_bg_numeric_test(void)
{
    static unsigned char const pam_rgba_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '4', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ',
        'R', 'G', 'B', '_', 'A', 'L', 'P', 'H', 'A', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        128u, 64u, 32u, 128u
    };
    static unsigned char const src_rgb[3] = { 128u, 64u, 32u };
    static unsigned char const src_alpha = 128u;
    static unsigned char const bgcolor_linear_u8[3] = { 64u, 128u, 192u };
    static float const bg_black[3] = { 0.0f, 0.0f, 0.0f };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe_black;
    pnm_numeric_probe_context_t probe_bg;
    SIXELSTATUS status;
    float expected_black[3];
    float expected_bg[3];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe_black, 0, sizeof(probe_black));
    memset(&probe_bg, 0, sizeof(probe_bg));
    memset(expected_black, 0, sizeof(expected_black));
    memset(expected_bg, 0, sizeof(expected_bg));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam rgba linear bg numeric (default black)",
        pam_rgba_sample,
        sizeof(pam_rgba_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe_black,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam rgba linear bg numeric: "
                "loader failed for default black (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }

    pnm_numeric_compose_expected_linear(expected_black,
                                        src_rgb,
                                        src_alpha,
                                        bg_black);
    result = verify_pnm_float_probe(
        "builtin loader pnm pam rgba linear bg numeric (default black)",
        &probe_black,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        1,
        1,
        expected_black,
        0.00001f);
    if (result != 0) {
        goto end;
    }

    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_linear_u8;
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam rgba linear bg numeric (linear bgcolor)",
        pam_rgba_sample,
        sizeof(pam_rgba_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe_bg,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam rgba linear bg numeric: "
                "loader failed for linear bgcolor (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }

    bg_linear[0] = (float)bgcolor_linear_u8[0] / 255.0f;
    bg_linear[1] = (float)bgcolor_linear_u8[1] / 255.0f;
    bg_linear[2] = (float)bgcolor_linear_u8[2] / 255.0f;
    pnm_numeric_compose_expected_linear(expected_bg,
                                        src_rgb,
                                        src_alpha,
                                        bg_linear);
    result = verify_pnm_float_probe(
        "builtin loader pnm pam rgba linear bg numeric (linear bgcolor)",
        &probe_bg,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        1,
        1,
        expected_bg,
        0.00001f);
    if (result != 0) {
        goto end;
    }

    for (channel = 0; channel < 3; ++channel) {
        if (!float_approx_equal(probe_black.pixels_f32[channel],
                                probe_bg.pixels_f32[channel],
                                0.000001f)) {
            result = 0;
            goto end;
        }
    }
    fprintf(stderr,
            "builtin loader pnm pam rgba linear bg numeric: "
            "bgcolor did not affect composed output\n");
    result = 1;

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_pnm_ppm16_float32_numeric_test(void)
{
    static unsigned char const ppm16_sample[] = {
        'P', '6', '\n',
        '1', ' ', '1', '\n',
        '6', '5', '5', '3', '5', '\n',
        0xffu, 0xffu,
        0x80u, 0x00u,
        0x00u, 0x01u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_rgb[3];
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_rgb, 0, sizeof(expected_rgb));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm ppm16 float32 numeric",
        ppm16_sample,
        sizeof(ppm16_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm ppm16 float32 numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }

    expected_rgb[0] = 1.0f;
    expected_rgb[1] = 32768.0f / 65535.0f;
    expected_rgb[2] = 1.0f / 65535.0f;
    result = verify_pnm_float_probe(
        "builtin loader pnm ppm16 float32 numeric",
        &probe,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        1,
        1,
        expected_rgb,
        0.000001f);

    return result;
}

static int
run_builtin_loader_pnm_ppm8_fastpath_numeric_test(void)
{
    static unsigned char const ppm8_sample[] = {
        'P', '6', '\n',
        '2', ' ', '1', '\n',
        '2', '5', '5', '\n',
        1u, 2u, 3u, 4u, 5u, 6u
    };
    static unsigned char const expected_rgb[6] = { 1u, 2u, 3u, 4u, 5u, 6u };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm ppm8 fastpath numeric",
        ppm8_sample,
        sizeof(ppm8_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm ppm8 fastpath numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm ppm8 fastpath numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm ppm8 fastpath numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader pnm ppm8 fastpath numeric: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width != 2 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm ppm8 fastpath numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (memcmp(probe.pixels_u8, expected_rgb, sizeof(expected_rgb)) != 0) {
        fprintf(stderr,
                "builtin loader pnm ppm8 fastpath numeric: "
                "RGB samples mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_unknown_key_compat_numeric_test(void)
{
    static unsigned char const pam_unknown_key_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'F', 'O', 'O', ' ', 'B', 'A', 'R', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ',
        'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        10u, 20u, 30u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam unknown key compatibility",
        pam_unknown_key_sample,
        sizeof(pam_unknown_key_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam unknown key compatibility: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam unknown key compatibility: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam unknown key compatibility: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader pnm pam unknown key compatibility: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width != 1 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm pam unknown key compatibility: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.pixels_u8[0] != 10u ||
        probe.pixels_u8[1] != 20u ||
        probe.pixels_u8[2] != 30u) {
        fprintf(stderr,
                "builtin loader pnm pam unknown key compatibility: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_unknown_long_key_compat_test(void)
{
    static char const pam_header[] =
        "P7\n"
        "WIDTH 1\n"
        "HEIGHT 1\n"
        "DEPTH 3\n"
        "MAXVAL 255\n"
        "THIS_IS_A_VERY_LONG_UNKNOWN_PAM_HEADER_KEY_NAME_"
        "THAT_EXCEEDS_TOKEN_LIMIT SOMEVALUE\n"
        "TUPLTYPE RGB\n"
        "ENDHDR\n";
    unsigned char pam_unknown_long_key_sample[sizeof(pam_header) - 1u + 3u];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    size_t header_size;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(pam_unknown_long_key_sample, 0, sizeof(pam_unknown_long_key_sample));
    header_size = 0u;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    header_size = sizeof(pam_header) - 1u;
    memcpy(pam_unknown_long_key_sample, pam_header, header_size);
    pam_unknown_long_key_sample[header_size + 0u] = 10u;
    pam_unknown_long_key_sample[header_size + 1u] = 20u;
    pam_unknown_long_key_sample[header_size + 2u] = 30u;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam unknown long key compatibility",
        pam_unknown_long_key_sample,
        sizeof(pam_unknown_long_key_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key compatibility: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key compatibility: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key compatibility: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key compatibility: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width != 1 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key compatibility: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.pixels_u8[0] != 10u ||
        probe.pixels_u8[1] != 20u ||
        probe.pixels_u8[2] != 30u) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key compatibility: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
SIXEL_T0014_FN(SIXEL_T0014_PREF_PNM_PAM_ULK_STRICT_REG, _test)(void)
{
    static char const pam_missing_required_sample[] =
        "P7\n"
        "WIDTH 1\n"
        "HEIGHT 1\n"
        "DEPTH 3\n"
        "THIS_IS_A_VERY_LONG_UNKNOWN_PAM_HEADER_KEY_NAME_"
        "THAT_EXCEEDS_TOKEN_LIMIT SOMEVALUE\n"
        "TUPLTYPE RGB\n"
        "ENDHDR\n";
    static char const pam_known_key_reject_sample[] =
        "P7\n"
        "WIDTH 1 EXTRA\n"
        "HEIGHT 1\n"
        "DEPTH 3\n"
        "MAXVAL 255\n"
        "THIS_IS_A_VERY_LONG_UNKNOWN_PAM_HEADER_KEY_NAME_"
        "THAT_EXCEEDS_TOKEN_LIMIT SOMEVALUE\n"
        "TUPLTYPE RGB\n"
        "ENDHDR\n";
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam unknown long key missing required",
        (unsigned char const *)pam_missing_required_sample,
        sizeof(pam_missing_required_sample) - 1u,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key missing required: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "PAM header is missing required fields") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key missing required: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam unknown long key known-field reject",
        (unsigned char const *)pam_known_key_reject_sample,
        sizeof(pam_known_key_reject_sample) - 1u,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key known-field reject: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unexpected token after WIDTH value") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam unknown long key known-field reject: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
pnm_test_append_text(unsigned char *buffer,
                     size_t capacity,
                     size_t *offset,
                     char const *text)
{
    size_t text_size;

    text_size = 0u;
    if (buffer == NULL || offset == NULL || text == NULL) {
        return 0;
    }
    text_size = strlen(text);
    if (capacity < *offset || text_size > capacity - *offset) {
        return 0;
    }
    memcpy(buffer + *offset, text, text_size);
    *offset += text_size;
    return 1;
}

static int
pnm_build_large_header_bytes_target_sample(unsigned char *buffer,
                                           size_t capacity,
                                           size_t target_header_bytes,
                                           size_t *sample_size)
{
    static char const prefix[] =
        "P7\n"
        "WIDTH 1\n"
        "HEIGHT 1\n"
        "DEPTH 3\n"
        "MAXVAL 255\n";
    static char const suffix[] =
        " 0\n"
        "TUPLTYPE RGB\n"
        "ENDHDR\n";
    size_t offset;
    size_t current_header_bytes;
    size_t suffix_size;
    size_t key_length;
    size_t i;

    offset = 0u;
    current_header_bytes = 0u;
    suffix_size = 0u;
    key_length = 0u;
    i = 0u;

    if (buffer == NULL || sample_size == NULL) {
        return 0;
    }

    if (!pnm_test_append_text(buffer, capacity, &offset, prefix)) {
        return 0;
    }
    if (offset < 2u) {
        return 0;
    }
    current_header_bytes = offset - 2u;
    suffix_size = sizeof(suffix) - 1u;
    if (target_header_bytes <= current_header_bytes + suffix_size) {
        return 0;
    }
    key_length = target_header_bytes - current_header_bytes - suffix_size;
    if (capacity < offset || key_length > capacity - offset) {
        return 0;
    }
    for (i = 0u; i < key_length; ++i) {
        buffer[offset++] = 'L';
    }
    if (!pnm_test_append_text(buffer, capacity, &offset, suffix)) {
        return 0;
    }
    if (offset < 2u || offset - 2u != target_header_bytes) {
        return 0;
    }
    if (capacity < offset || 3u > capacity - offset) {
        return 0;
    }
    buffer[offset + 0u] = 10u;
    buffer[offset + 1u] = 20u;
    buffer[offset + 2u] = 30u;
    offset += 3u;

    *sample_size = offset;
    return 1;
}

static int
pnm_build_large_header_bytes_sample(unsigned char *buffer,
                                    size_t capacity,
                                    size_t *sample_size)
{
    return pnm_build_large_header_bytes_target_sample(
        buffer,
        capacity,
        66000u,
        sample_size);
}

static int
pnm_build_large_header_lines_target_sample(unsigned char *buffer,
                                           size_t capacity,
                                           size_t target_header_lines,
                                           size_t *sample_size)
{
    static char const prefix[] =
        "P7\n"
        "WIDTH 1\n"
        "HEIGHT 1\n"
        "DEPTH 3\n"
        "MAXVAL 255\n";
    static char const unknown_line[] = "U 0\n";
    static char const suffix[] =
        "TUPLTYPE RGB\n"
        "ENDHDR\n";
    size_t offset;
    size_t unknown_lines;
    size_t base_lines;
    size_t i;

    offset = 0u;
    unknown_lines = 0u;
    base_lines = 7u;
    i = 0u;

    if (buffer == NULL || sample_size == NULL) {
        return 0;
    }
    if (target_header_lines < base_lines) {
        return 0;
    }

    unknown_lines = target_header_lines - base_lines;
    if (!pnm_test_append_text(buffer, capacity, &offset, prefix)) {
        return 0;
    }
    for (i = 0u; i < unknown_lines; ++i) {
        if (!pnm_test_append_text(buffer, capacity, &offset, unknown_line)) {
            return 0;
        }
    }
    if (!pnm_test_append_text(buffer, capacity, &offset, suffix)) {
        return 0;
    }
    if (capacity < offset || 3u > capacity - offset) {
        return 0;
    }
    buffer[offset + 0u] = 10u;
    buffer[offset + 1u] = 20u;
    buffer[offset + 2u] = 30u;
    offset += 3u;

    *sample_size = offset;
    return 1;
}

static int
pnm_build_large_header_lines_sample(unsigned char *buffer,
                                    size_t capacity,
                                    size_t *sample_size)
{
    return pnm_build_large_header_lines_target_sample(
        buffer,
        capacity,
        1037u,
        sample_size);
}

static int
pnm_assert_rgb888_single_pixel_probe(pnm_numeric_probe_context_t const *probe,
                                     char const *label)
{
    if (probe == NULL || label == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr,
                "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (probe->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if (probe->width != 1 || probe->height != 1) {
        fprintf(stderr,
                "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (probe->pixels_u8[0] != 10u ||
        probe->pixels_u8[1] != 20u ||
        probe->pixels_u8[2] != 30u) {
        fprintf(stderr,
                "%s: RGB mismatch (%u,%u,%u)\n",
                label,
                (unsigned int)probe->pixels_u8[0],
                (unsigned int)probe->pixels_u8[1],
                (unsigned int)probe->pixels_u8[2]);
        return 1;
    }
    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_bytes_strict_test(void)
{
    unsigned char sample[70000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    size_t sample_size;
    int result;

    memset(sample, 0, sizeof(sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_bytes_sample(sample,
                                             sizeof(sample),
                                             &sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes strict: "
                "failed to build sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header bytes strict",
        sample,
        sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "PAM header bytes exceed limit") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_bytes_compat_test(void)
{
    unsigned char sample[70000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    size_t sample_size;
    int result;

    memset(sample, 0, sizeof(sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_bytes_sample(sample,
                                             sizeof(sample),
                                             &sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "failed to build sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header bytes compat",
        sample,
        sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 1 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.pixels_u8[0] != 10u ||
        probe.pixels_u8[1] != 20u ||
        probe.pixels_u8[2] != 30u) {
        fprintf(stderr,
                "builtin loader pnm pam large header bytes compat: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_lines_strict_test(void)
{
    unsigned char sample[12000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    size_t sample_size;
    int result;

    memset(sample, 0, sizeof(sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_lines_sample(sample,
                                             sizeof(sample),
                                             &sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines strict: "
                "failed to build sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header lines strict",
        sample,
        sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "PAM header lines exceed limit") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_lines_compat_test(void)
{
    unsigned char sample[12000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    size_t sample_size;
    int result;

    memset(sample, 0, sizeof(sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_lines_sample(sample,
                                             sizeof(sample),
                                             &sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "failed to build sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header lines compat",
        sample,
        sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 1 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.pixels_u8[0] != 10u ||
        probe.pixels_u8[1] != 20u ||
        probe.pixels_u8[2] != 30u) {
        fprintf(stderr,
                "builtin loader pnm pam large header lines compat: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_boundary_strict_test(void)
{
    unsigned char bytes_sample[70000];
    unsigned char lines_sample[12000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    size_t bytes_sample_size;
    size_t lines_sample_size;
    int result;

    memset(bytes_sample, 0, sizeof(bytes_sample));
    memset(lines_sample, 0, sizeof(lines_sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    bytes_sample_size = 0u;
    lines_sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_bytes_target_sample(bytes_sample,
                                                    sizeof(bytes_sample),
                                                    65536u,
                                                    &bytes_sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header boundary strict: "
                "failed to build bytes sample\n");
        return 1;
    }
    if (!pnm_build_large_header_lines_target_sample(lines_sample,
                                                    sizeof(lines_sample),
                                                    1024u,
                                                    &lines_sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header boundary strict: "
                "failed to build lines sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header boundary strict: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header boundary strict bytes",
        bytes_sample,
        bytes_sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header boundary strict bytes: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (pnm_assert_rgb888_single_pixel_probe(
            &probe,
            "builtin loader pnm pam large header boundary strict bytes") != 0) {
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header boundary strict lines",
        lines_sample,
        lines_sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header boundary strict lines: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (pnm_assert_rgb888_single_pixel_probe(
            &probe,
            "builtin loader pnm pam large header boundary strict lines") != 0) {
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_over_bytes_strict_test(void)
{
    unsigned char sample[70000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    size_t sample_size;
    int result;

    memset(sample, 0, sizeof(sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_bytes_target_sample(sample,
                                                    sizeof(sample),
                                                    65537u,
                                                    &sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-bytes strict: "
                "failed to build sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-bytes strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header over-bytes strict",
        sample,
        sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-bytes strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "PAM header bytes exceed limit") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-bytes strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_over_lines_strict_test(void)
{
    unsigned char sample[12000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    size_t sample_size;
    int result;

    memset(sample, 0, sizeof(sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_lines_target_sample(sample,
                                                    sizeof(sample),
                                                    1025u,
                                                    &sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-lines strict: "
                "failed to build sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-lines strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header over-lines strict",
        sample,
        sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-lines strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "PAM header lines exceed limit") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam large header over-lines strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_large_header_overlimit_compat_test(void)
{
    unsigned char bytes_sample[70000];
    unsigned char lines_sample[12000];
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    size_t bytes_sample_size;
    size_t lines_sample_size;
    int result;

    memset(bytes_sample, 0, sizeof(bytes_sample));
    memset(lines_sample, 0, sizeof(lines_sample));
    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    bytes_sample_size = 0u;
    lines_sample_size = 0u;
    result = 1;

    if (!pnm_build_large_header_bytes_target_sample(bytes_sample,
                                                    sizeof(bytes_sample),
                                                    65537u,
                                                    &bytes_sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat: "
                "failed to build bytes sample\n");
        return 1;
    }
    if (!pnm_build_large_header_lines_target_sample(lines_sample,
                                                    sizeof(lines_sample),
                                                    1025u,
                                                    &lines_sample_size)) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat: "
                "failed to build lines sample\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat: "
                "setenv bytes failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header overlimit compat bytes",
        bytes_sample,
        bytes_sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat: "
                "reset env after bytes failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat bytes: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (pnm_assert_rgb888_single_pixel_probe(
            &probe,
            "builtin loader pnm pam large header overlimit compat "
            "bytes") != 0) {
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat: "
                "setenv lines failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam large header overlimit compat lines",
        lines_sample,
        lines_sample_size,
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat: "
                "reset env after lines failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam large header overlimit compat lines: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (pnm_assert_rgb888_single_pixel_probe(
            &probe,
            "builtin loader pnm pam large header overlimit compat "
            "lines") != 0) {
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_ascii_truncated_strict_test(void)
{
    static unsigned char const truncated_ascii_p3[] = {
        'P', '3', '\n',
        '2', ' ', '1', '\n',
        '2', '5', '5', '\n',
        '2', '5', '5', ' ', '0', ' ', '0', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm ascii truncated strict",
        truncated_ascii_p3,
        sizeof(truncated_ascii_p3),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated strict: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unexpected end of ASCII raster") == NULL) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_ascii_truncated_compat_test(void)
{
    static unsigned char const truncated_ascii_p3[] = {
        'P', '3', '\n',
        '2', ' ', '1', '\n',
        '2', '5', '5', '\n',
        '2', '5', '5', ' ', '0', ' ', '0', '\n'
    };
    static unsigned char const expected_rgb[6] = {
        255u, 0u, 0u, 0u, 0u, 0u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII",
                           "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm ascii truncated compat",
        truncated_ascii_p3,
        sizeof(truncated_ascii_p3),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 2 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (memcmp(probe.pixels_u8, expected_rgb, sizeof(expected_rgb)) != 0) {
        fprintf(stderr,
                "builtin loader pnm ascii truncated compat: "
                "RGB mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_signature_guard_test(void)
{
    static unsigned char const malformed_pnm_signature[] = {
        'P', '6', 'x', ' ', 'n', 'o', 't', '-', 'p', 'n', 'm', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm signature guard",
        malformed_pnm_signature,
        sizeof(malformed_pnm_signature),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm signature guard: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message != NULL && strstr(message, "load_pnm:") != NULL) {
        fprintf(stderr,
                "builtin loader pnm signature guard: "
                "unexpected pnm decode path (%s)\n",
                message);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_known_key_extra_token_reject_test(void)
{
    static unsigned char const pam_extra_token_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', ' ', 'X', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ',
        'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam known key extra token reject",
        pam_extra_token_sample,
        sizeof(pam_extra_token_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam known key extra token reject: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unexpected token after WIDTH value") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam known key extra token reject: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_tupletype_duplicate_last_wins_test(void)
{
    static unsigned char const pam_tuple_duplicate_accept[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B',
        '_', 'A', 'L', 'P', 'H', 'A', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        11u, 22u, 33u
    };
    static unsigned char const pam_tuple_duplicate_reject[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B',
        '_', 'A', 'L', 'P', 'H', 'A', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam tupletype duplicate last wins accept",
        pam_tuple_duplicate_accept,
        sizeof(pam_tuple_duplicate_accept),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam tupletype duplicate accept: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam tupletype duplicate accept: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam tupletype duplicate accept: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.pixels_u8[0] != 11u ||
        probe.pixels_u8[1] != 22u ||
        probe.pixels_u8[2] != 33u) {
        fprintf(stderr,
                "builtin loader pnm pam tupletype duplicate accept: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam tupletype duplicate last wins reject",
        pam_tuple_duplicate_reject,
        sizeof(pam_tuple_duplicate_reject),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam tupletype duplicate reject: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "PAM TUPLTYPE/DEPTH mismatch") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam tupletype duplicate reject: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
SIXEL_T0014_FN(SIXEL_T0014_PREF_PNM_PAM_UT_FALLBACK_BND, _test)(void)
{
    static unsigned char const pam_unknown_tuple_depth4[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '4', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ',
        'C', 'U', 'S', 'T', 'O', 'M', '_', 'R', 'G', 'B', 'A', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        100u, 50u, 25u, 255u
    };
    static unsigned char const pam_unknown_tuple_depth5[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '5', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ',
        'C', 'U', 'S', 'T', 'O', 'M', '_', '5', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam unknown tupletype fallback depth4",
        pam_unknown_tuple_depth4,
        sizeof(pam_unknown_tuple_depth4),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam unknown tuple depth4: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam unknown tuple depth4: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam unknown tuple depth4: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.pixels_u8[0] != 100u ||
        probe.pixels_u8[1] != 50u ||
        probe.pixels_u8[2] != 25u) {
        fprintf(stderr,
                "builtin loader pnm pam unknown tuple depth4: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam unknown tupletype fallback depth5",
        pam_unknown_tuple_depth5,
        sizeof(pam_unknown_tuple_depth5),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam unknown tuple depth5: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unknown TUPLTYPE fallback") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam unknown tuple depth5: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_duplicate_required_key_strict_test(void)
{
    static unsigned char const pam_duplicate_width_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '2', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
                           "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate required key strict",
        pam_duplicate_width_sample,
        sizeof(pam_duplicate_width_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key strict: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "duplicate PAM WIDTH key") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_duplicate_required_key_compat_test(void)
{
    static unsigned char const pam_duplicate_width_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '2', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        1u, 2u, 3u, 4u, 5u, 6u
    };
    static unsigned char const expected_rgb[6] = {
        1u, 2u, 3u, 4u, 5u, 6u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
                           "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate required key compat",
        pam_duplicate_width_sample,
        sizeof(pam_duplicate_width_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
                           "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 2 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (memcmp(probe.pixels_u8, expected_rgb, sizeof(expected_rgb)) != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required key compat: "
                "RGB mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_tuple_dup_compat_regression_test(void)
{
    return run_builtin_loader_pnm_pam_tupletype_duplicate_last_wins_test();
}

static int
run_builtin_loader_pnm_pam_endhdr_trailing_token_strict_test(void)
{
    static unsigned char const pam_endhdr_trailing_token_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', ' ', 'E', 'X', 'T', 'R', 'A', '\n',
        1u, 2u, 3u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv(
            "SIXEL_LOADER_PAM_ALLOW_ENDHDR_TRAILING_TOKENS", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam endhdr trailing token strict",
        pam_endhdr_trailing_token_sample,
        sizeof(pam_endhdr_trailing_token_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token strict: "
                "unexpected success\n");
        return 1;
    }

    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unexpected token after ENDHDR") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_endhdr_trailing_token_compat_test(void)
{
    static unsigned char const pam_endhdr_trailing_token_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', ' ', 'E', 'X', 'T', 'R', 'A', '\n',
        1u, 2u, 3u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv(
            "SIXEL_LOADER_PAM_ALLOW_ENDHDR_TRAILING_TOKENS", "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam endhdr trailing token compat",
        pam_endhdr_trailing_token_sample,
        sizeof(pam_endhdr_trailing_token_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv(
            "SIXEL_LOADER_PAM_ALLOW_ENDHDR_TRAILING_TOKENS", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.width != 1 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.pixels_u8[0] != 1u ||
        probe.pixels_u8[1] != 2u ||
        probe.pixels_u8[2] != 3u) {
        fprintf(stderr,
                "builtin loader pnm pam endhdr trailing token compat: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_dup_required_keys_remain_strict_test(void)
{
    static unsigned char const pam_duplicate_height_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '2', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    static unsigned char const pam_duplicate_depth_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '4', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    static unsigned char const pam_duplicate_maxval_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '1', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
                           "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required remaining strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate height strict",
        pam_duplicate_height_sample,
        sizeof(pam_duplicate_height_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate height strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "duplicate PAM HEIGHT key") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate height strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate depth strict",
        pam_duplicate_depth_sample,
        sizeof(pam_duplicate_depth_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate depth strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL || strstr(message, "duplicate PAM DEPTH key") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate depth strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate maxval strict",
        pam_duplicate_maxval_sample,
        sizeof(pam_duplicate_maxval_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate maxval strict: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "duplicate PAM MAXVAL key") == NULL) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate maxval strict: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_pam_dup_required_keys_remain_compat_test(void)
{
    static unsigned char const pam_duplicate_height_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '2', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        1u, 2u, 3u, 4u, 5u, 6u
    };
    static unsigned char const pam_duplicate_depth_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        7u, 8u, 9u
    };
    static unsigned char const pam_duplicate_maxval_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '1', '5', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        10u, 20u, 30u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
                           "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required remaining compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate height compat",
        pam_duplicate_height_sample,
        sizeof(pam_duplicate_height_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.width != 1 || probe.height != 2 ||
        probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate height compat: "
                "unexpected decode result\n");
        return 1;
    }
    if (probe.pixels_u8[0] != 1u ||
        probe.pixels_u8[1] != 2u ||
        probe.pixels_u8[2] != 3u ||
        probe.pixels_u8[3] != 4u ||
        probe.pixels_u8[4] != 5u ||
        probe.pixels_u8[5] != 6u) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate height compat: "
                "RGB mismatch\n");
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate depth compat",
        pam_duplicate_depth_sample,
        sizeof(pam_duplicate_depth_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.width != 1 || probe.height != 1 ||
        probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate depth compat: "
                "unexpected decode result\n");
        return 1;
    }
    if (probe.pixels_u8[0] != 7u ||
        probe.pixels_u8[1] != 8u ||
        probe.pixels_u8[2] != 9u) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate depth compat: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm pam duplicate maxval compat",
        pam_duplicate_maxval_sample,
        sizeof(pam_duplicate_maxval_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS",
                           "") != 0) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate required remaining compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.width != 1 || probe.height != 1 ||
        probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate maxval compat: "
                "unexpected decode result\n");
        return 1;
    }
    if (probe.pixels_u8[0] != 10u ||
        probe.pixels_u8[1] != 20u ||
        probe.pixels_u8[2] != 30u) {
        fprintf(stderr,
                "builtin loader pnm pam duplicate maxval compat: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_trailing_data_strict_test(void)
{
    static unsigned char const ppm_trailing_sample[] = {
        'P', '6', '\n',
        '1', ' ', '1', '\n',
        '2', '5', '5', '\n',
        10u, 20u, 30u, 0u
    };
    static unsigned char const pam_trailing_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        1u, 2u, 3u, 0u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    char const *message;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    message = NULL;
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm trailing data strict: "
                "setenv failed\n");
        return 1;
    }

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm trailing data strict ppm",
        ppm_trailing_sample,
        sizeof(ppm_trailing_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm trailing data strict ppm: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unexpected trailing raster data") == NULL) {
        fprintf(stderr,
                "builtin loader pnm trailing data strict ppm: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm trailing data strict pam",
        pam_trailing_sample,
        sizeof(pam_trailing_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader pnm trailing data strict pam: "
                "unexpected success\n");
        return 1;
    }
    message = sixel_helper_get_additional_message();
    if (message == NULL ||
        strstr(message, "unexpected trailing raster data") == NULL) {
        fprintf(stderr,
                "builtin loader pnm trailing data strict pam: "
                "unexpected message (%s)\n",
                message != NULL ? message : "(null)");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_trailing_data_compat_test(void)
{
    static unsigned char const ppm_trailing_sample[] = {
        'P', '6', '\n',
        '1', ' ', '1', '\n',
        '2', '5', '5', '\n',
        10u, 20u, 30u, 0u
    };
    static unsigned char const pam_trailing_sample[] = {
        'P', '7', '\n',
        'W', 'I', 'D', 'T', 'H', ' ', '1', '\n',
        'H', 'E', 'I', 'G', 'H', 'T', ' ', '1', '\n',
        'D', 'E', 'P', 'T', 'H', ' ', '3', '\n',
        'M', 'A', 'X', 'V', 'A', 'L', ' ', '2', '5', '5', '\n',
        'T', 'U', 'P', 'L', 'T', 'Y', 'P', 'E', ' ', 'R', 'G', 'B', '\n',
        'E', 'N', 'D', 'H', 'D', 'R', '\n',
        1u, 2u, 3u, 0u
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA",
                           "1") != 0) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm trailing data compat ppm",
        ppm_trailing_sample,
        sizeof(ppm_trailing_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat ppm: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat ppm: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat ppm: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.pixels_u8[0] != 10u ||
        probe.pixels_u8[1] != 20u ||
        probe.pixels_u8[2] != 30u) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat ppm: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    memset(&probe, 0, sizeof(probe));
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm trailing data compat pam",
        pam_trailing_sample,
        sizeof(pam_trailing_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat pam: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat pam: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat pam: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.pixels_u8[0] != 1u ||
        probe.pixels_u8[1] != 2u ||
        probe.pixels_u8[2] != 3u) {
        fprintf(stderr,
                "builtin loader pnm trailing data compat pam: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_pnm_ascii_trailing_comment_strict_test(void)
{
    static unsigned char const ascii_trailing_comment_sample[] = {
        'P', '3', '\n',
        '1', ' ', '1', '\n',
        '2', '5', '5', '\n',
        '1', ' ', '2', ' ', '3', '\n',
        ' ', ' ', '#', 't', 'a', 'i', 'l', '\n',
        ' ', '\t', '\r', '\n'
    };
    builtin_loader_probe_options_t options;
    pnm_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    if (loader_test_setenv("SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA", "") != 0) {
        fprintf(stderr,
                "builtin loader pnm ascii trailing comment strict: "
                "setenv failed\n");
        return 1;
    }

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader pnm ascii trailing comment strict",
        ascii_trailing_comment_sample,
        sizeof(ascii_trailing_comment_sample),
        &options,
        capture_pnm_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader pnm ascii trailing comment strict: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader pnm ascii trailing comment strict: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader pnm ascii trailing comment strict: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.pixels_u8[0] != 1u ||
        probe.pixels_u8[1] != 2u ||
        probe.pixels_u8[2] != 3u) {
        fprintf(stderr,
                "builtin loader pnm ascii trailing comment strict: "
                "RGB mismatch (%u,%u,%u)\n",
                (unsigned int)probe.pixels_u8[0],
                (unsigned int)probe.pixels_u8[1],
                (unsigned int)probe.pixels_u8[2]);
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
          SIXEL_T0014_FN(SIXEL_T0014_PREF_HDR_GINV_PVALID_NUM, _test) },
        { "SIXEL_TEST_HDR_NUMERIC_PRIMARIES_INVALID_GAMMA_VALID",
          SIXEL_T0014_FN(SIXEL_T0014_PREF_HDR_PINV_GVALID_NUM, _test) },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_HEADER_EXPOSURE",
          run_builtin_loader_hdr_invalid_header_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_MIXED_HEADER_EXPOSURE_INVALID",
          SIXEL_T0014_FN(SIXEL_T0014_PREF_HDR_MIX_HDR_EXP_INV_NUM, _test) },
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
    static builtin_loader_env_dispatch_entry_t const bmp_env_dispatch[] = {
        { "SIXEL_TEST_BMP_NUMERIC_RGBA_BGCOLOR_FLOAT32",
          run_builtin_loader_bmp_rgba_bgcolor_float32_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RGBA_MASK_NO_BG",
          run_builtin_loader_bmp_rgba_mask_no_bg_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OPAQUE_FASTPATH",
          run_builtin_loader_bmp_opaque_fastpath_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RLE8_DECODE",
          run_builtin_loader_bmp_rle8_decode_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RLE4_DECODE",
          run_builtin_loader_bmp_rle4_decode_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RLE_BROKEN_FAIL",
          run_builtin_loader_bmp_rle_broken_fail_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_CORE12_1BPP_PALETTE",
          run_builtin_loader_bmp_core12_1bpp_palette_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_4BPP_PALETTE",
          run_builtin_loader_bmp_info40_4bpp_palette_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BITFIELDS_16_RGB565",
          run_builtin_loader_bmp_bitfields_rgb565_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BITFIELDS_32_ALPHA_MASK_NO_BG",
          run_builtin_loader_bmp_bitfields_alpha_mask_no_bg_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V4_32_ALPHA_BGCOLOR_FLOAT32",
          run_builtin_loader_bmp_v4_alpha_bgcolor_float32_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_TOPDOWN_24BPP",
          run_builtin_loader_bmp_topdown_24bpp_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RLE8_ABSOLUTE_PADDING",
          run_builtin_loader_bmp_rle8_absolute_padding_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE8_DELTA_TOPDOWN",
          run_builtin_loader_bmp_fail_rle8_delta_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RLE4_MIXED",
          run_builtin_loader_bmp_rle4_mixed_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_RLE4_INVALID_DELTA_FAIL",
          run_builtin_loader_bmp_rle4_invalid_delta_fail_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_8BPP_PALETTE",
          run_builtin_loader_bmp_info40_8bpp_palette_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_TOPDOWN_1BPP_PALETTE",
          run_builtin_loader_bmp_info40_topdown_1bpp_palette_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_16BPP_RGB555",
          run_builtin_loader_bmp_info40_16bpp_rgb555_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_32BPP_ALPHA_ZERO_OPAQUE",
          run_builtin_loader_bmp_info40_32bpp_a0_opaque_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_V5_32_ALPHA_BGCOLOR_FLOAT32",
          run_builtin_loader_bmp_v5_alpha_bgcolor_float32_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_UNSUPPORTED_DIB_SIZE",
          run_builtin_loader_bmp_fail_unsupported_dib_size_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE8_REQUIRES_8BPP",
          run_builtin_loader_bmp_fail_rle8_requires_8bpp_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE4_REQUIRES_4BPP",
          run_builtin_loader_bmp_fail_rle4_requires_4bpp_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_INVALID_COLOR_MASKS",
          run_builtin_loader_bmp_fail_invalid_color_masks_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE8_MISSING_END_MARKER",
          run_builtin_loader_bmp_fail_rle8_no_eom_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_8BPP_COLORS_USED_PALETTE",
          run_builtin_loader_bmp_info40_8bpp_palette_used_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_32BPP_BITFIELDS_NO_ALPHA",
          run_builtin_loader_bmp_info40_32bpp_bf_no_alpha_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_V3_32_ALPHA_BGCOLOR_FLOAT32",
          run_builtin_loader_bmp_v3_alpha_bgcolor_float32_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE8_TOPDOWN",
          run_builtin_loader_bmp_fail_rle8_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE4_TOPDOWN",
          run_builtin_loader_bmp_fail_rle4_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_TRUNCATED_MASKS",
          run_builtin_loader_bmp_fail_truncated_masks_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_TRUNCATED_PIXEL_DATA",
          run_builtin_loader_bmp_fail_truncated_pixel_data_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_PALETTE_INDEX_OVERFLOW",
          run_builtin_loader_bmp_fail_palette_index_overflow_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE8_ABSOLUTE_OVERFLOW",
          run_builtin_loader_bmp_fail_rle8_absolute_overflow_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_UNSUPPORTED_COMPRESSION",
          run_builtin_loader_bmp_fail_unsupported_comp_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_JPEG_CMS_OFF",
          run_builtin_loader_bmp_bi_jpeg_cms_off_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_JPEG_CMS_ON",
          run_builtin_loader_bmp_bi_jpeg_cms_on_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG_ALPHA_BGCOLOR",
          run_builtin_loader_bmp_bi_png_alpha_bgcolor_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG_ALPHA_MASK_NO_BG",
          run_builtin_loader_bmp_bi_png_alpha_mask_no_bg_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_BGCOLOR",
          run_builtin_loader_bmp_bi_png16_alpha_bgcolor_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_MASK_NO_BG",
          run_builtin_loader_bmp_bi_png16_alpha_mask_no_bg_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_MASK_NO_BG_CMS_ON",
          run_bmp_png16_cms_on_mask_t },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_MASK_NO_BG_CMS_ON_PREF8",
          run_bmp_png16_cms_on_pref8_t },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_OPAQUE_NO_BG_CMS_ON",
          run_bmp_png16_cms_on_opaque_t },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_BGCOLOR_CMS_ON",
          run_bmp_png16_bg_cms_on_t },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_MASK_NO_BG_ICC_CMS_ON",
          run_bmp_png16_icc_cms_on_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG_OPAQUE",
          run_builtin_loader_bmp_bi_png_opaque_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_BI_PNG_LINKED_OUTER_INNER_ICC",
          run_builtin_loader_bmp_bi_png_linked_icc_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_BI_JPEG_INVALID_PAYLOAD",
          run_builtin_loader_bmp_fail_bi_jpeg_payload_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_BI_PNG_PAYLOAD_RANGE",
          run_builtin_loader_bmp_fail_bi_png_payload_range_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V5_EMBEDDED_ICC_RGB_CMS_ON",
          run_builtin_loader_bmp_v5_embedded_icc_rgb_cms_on_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V5_EMBEDDED_ICC_RGB_CMS_OFF",
          run_builtin_loader_bmp_v5_embedded_icc_rgb_cms_off_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V5_EMBEDDED_ICC_RGBA_BGCOLOR",
          run_builtin_loader_bmp_v5_icc_rgba_bgcolor_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_V5_EMBEDDED_ICC_RGBA_MASK_NO_BG",
          run_builtin_loader_bmp_v5_icc_rgba_mask_no_bg_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_V5_LINKED_PROFILE_IGNORED",
          run_builtin_loader_bmp_v5_linked_profile_ignored_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_V5_EMBEDDED_ICC_RANGE",
          run_builtin_loader_bmp_fail_v5_embedded_icc_range_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_ALPHABITFIELDS_BGCOLOR_FLOAT32",
          run_builtin_loader_bmp_alphabitfields_bgcolor_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_ALPHABITFIELDS_MASK_NO_BG",
          run_builtin_loader_bmp_alphabitfields_mask_no_bg_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_ALPHABITFIELDS_INVALID_ALPHA_MASK",
          run_builtin_loader_bmp_fail_abf_invalid_mask_num_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_V4_CALIBRATED_CMS_ON",
          run_builtin_loader_bmp_v4_calibrated_cms_on_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V4_CALIBRATED_CMS_OFF",
          run_builtin_loader_bmp_v4_calibrated_cms_off_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V4_CALIBRATED_PROBE_SPLIT_GAMMA",
          run_builtin_loader_bmp_v4_cal_probe_split_gamma_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_V4_CALIBRATED_SPLIT_GAMMA_CMS_ON",
          run_bmp_v4_cal_split_gamma_cms_on_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_V5_EMBEDDED_ICC_CALIBRATED_PRIORITY",
          run_builtin_loader_bmp_v5_icc_calibrated_priority_num_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_CMYK_CMS_OFF",
          run_builtin_loader_bmp_cmyk_cms_off_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYK_TOPDOWN",
          run_builtin_loader_bmp_fail_cmyk_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_CMYK_EMBEDDED_ICC_CMS_ON",
          run_builtin_loader_bmp_cmyk_embedded_icc_cms_on_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYK_REQUIRES_32BPP",
          run_builtin_loader_bmp_fail_cmyk_requires_32bpp_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_CMYKRLE8_DECODE",
          run_builtin_loader_bmp_cmykrle8_decode_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_CMYKRLE4_DECODE",
          run_builtin_loader_bmp_cmykrle4_decode_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE8_REQUIRES_8BPP",
          run_builtin_loader_bmp_fail_cmykrle8_requires_8bpp_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE4_REQUIRES_4BPP",
          run_builtin_loader_bmp_fail_cmykrle4_requires_4bpp_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE8_BROKEN_STREAM",
          run_builtin_loader_bmp_fail_cmykrle8_broken_stream_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE4_BROKEN_STREAM",
          run_builtin_loader_bmp_fail_cmykrle4_broken_stream_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE8_TOPDOWN",
          run_builtin_loader_bmp_fail_cmykrle8_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE4_TOPDOWN",
          run_builtin_loader_bmp_fail_cmykrle4_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_RGB24_DECODE",
          run_builtin_loader_bmp_os2_rgb24_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_RLE8_DECODE",
          run_builtin_loader_bmp_os2_rle8_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_RLE4_DECODE",
          run_builtin_loader_bmp_os2_rle4_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_DECODE",
          run_builtin_loader_bmp_os2_huffman1d_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_RLE24_DECODE",
          run_builtin_loader_bmp_os2_rle24_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_RLE24_TRUNCATED",
          run_builtin_loader_bmp_fail_os2_rle24_truncated_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_INVALID_CODE",
          run_builtin_loader_bmp_fail_os2_huffman1d_invalid_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_RLE24_ABSOLUTE_OVERFLOW",
          run_builtin_loader_bmp_fail_os2_rle24_abs_ov_num_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_RLE24_DELTA_RANGE",
          run_builtin_loader_bmp_fail_os2_rle24_delta_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_RLE24_TOPDOWN",
          run_builtin_loader_bmp_fail_os2_rle24_topdown_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_LONG_RUN",
          run_builtin_loader_bmp_os2_huffman1d_long_run_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_MAKEUP_CHAIN",
          run_builtin_loader_bmp_os2_huffman1d_makeup_chain_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_BOUNDARY_128",
          run_builtin_loader_bmp_os2_huffman1d_boundary_128_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_MULTILINE_FILL",
          run_builtin_loader_bmp_os2_huff1d_multiline_fill_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_SHORT_RUN_COMPAT",
          run_builtin_loader_bmp_os2_huff1d_short_compat_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_MISSING_EOL",
          run_builtin_loader_bmp_fail_os2_huff1d_missing_eol_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_ROW_OVERFLOW",
          run_builtin_loader_bmp_fail_os2_huff1d_row_overflow_num_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_TRUNCATED_MAKEUP",
        run_builtin_loader_bmp_fail_os2_huff1d_trunc_makeup_num_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_INVALID_EOL",
          run_builtin_loader_bmp_fail_os2_huff1d_invalid_eol_num_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_INVALID_CODE2",
          run_builtin_loader_bmp_fail_os2_huff1d_invalid_code2_num_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_V2_16BPP_BITFIELDS_RGB565",
          run_builtin_loader_bmp_v2_16bpp_bitfields_rgb565_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V2_32BPP_BITFIELDS_NO_ALPHA",
          run_builtin_loader_bmp_v2_32bpp_bitfields_no_alpha_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_V2_16BPP_RGB555",
          run_builtin_loader_bmp_v2_16bpp_rgb555_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_V2_ALPHABITFIELDS",
          run_builtin_loader_bmp_fail_v2_alphabitfields_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_V2_TRUNCATED_MASKS",
          run_builtin_loader_bmp_fail_v2_truncated_masks_numeric_test },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_V2_ZERO_COLOR_MASKS",
          run_builtin_loader_bmp_fail_v2_zero_color_masks_numeric_test
        },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_SHORT16_RGB24",
          run_bmp_os2s16_rgb24_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_SHORT24_RLE8",
          run_bmp_os2s24_rle8_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_OS2_SHORT32_RLE4",
          run_bmp_os2s32_rle4_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_AUTO_HUFFMAN1D",
          run_bmp_i40_auto_huff_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_2BPP_PALETTE",
          run_bmp_i40_2bpp_pal_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_2BPP_PALETTE_TOPDOWN",
          run_bmp_i40_2bpp_pal_topdown_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_2BPP_PALETTE_OVERFLOW",
          run_bmp_i40_fail_2bpp_pal_ovf_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_WINDOWS_HUFFMAN1D_FAIL",
          run_bmp_i40_win_huff_fail_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_OS2_HUFFMAN1D",
          run_bmp_i40_os2_huff_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_AUTO_RLE24",
          run_bmp_i40_auto_r24_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_WINDOWS_RLE24_FAIL",
          run_bmp_i40_win_r24_fail_t
        },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_WINDOWS_COMP14_FAIL",
          run_bmp_i40_win_c14_fail_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_WINDOWS_COMP15_FAIL",
          run_bmp_i40_win_c15_fail_t
        },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_RGB32_IMAGE_SIZE_OVERFLOW",
          run_bmp_i40_fail_rgb32_imgovf_t },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_PIXEL_OFFSET_EQUALS_FILE_SIZE",
          run_bmp_i40_fail_pixoff_eqsize_t },
        { "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_PALETTE_OFFSET",
          run_bmp_i40_fail_paloff_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_ABF_ALLZERO_ALPHA_MASK_NO_BG",
          run_bmp_i40_abf_a0_mask_num_t },
        { "SIXEL_TEST_BMP_NUMERIC_INFO40_BF_A0_EXPLICIT_MASK_NO_BG",
          run_bmp_i40_bf_a0_mask_num_t
        }
    };
    static builtin_loader_env_dispatch_entry_t const tga_env_dispatch[] = {
        { "SIXEL_TEST_TGA_NUMERIC_RGBA_ALPHA_MASK_BGCOLOR",
          run_builtin_loader_tga_rgba_alpha_mask_bgcolor_numeric_test },
        { "SIXEL_TEST_TGA_NUMERIC_PAL_RGBA_TRANSPARENT_INDEX",
          run_builtin_loader_tga_pal_rgba_transparent_index_numeric_test }
    };
    static builtin_loader_env_dispatch_entry_t const pnm_env_dispatch[] = {
        { "SIXEL_TEST_PNM_NUMERIC_PAM_RGBA_LINEAR_BG",
          run_builtin_loader_pnm_pam_rgba_linear_bg_numeric_test },
        { "SIXEL_TEST_PNM_NUMERIC_PPM16_FLOAT32",
          run_builtin_loader_pnm_ppm16_float32_numeric_test },
        { "SIXEL_TEST_PNM_NUMERIC_PPM8_FASTPATH",
          run_builtin_loader_pnm_ppm8_fastpath_numeric_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_UNKNOWN_KEY_COMPAT",
          run_builtin_loader_pnm_pam_unknown_key_compat_numeric_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_UNKNOWN_LONG_KEY_COMPAT",
          run_builtin_loader_pnm_pam_unknown_long_key_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_UNKNOWN_LONG_KEY_STRICT_REGRESSION",
          SIXEL_T0014_FN(SIXEL_T0014_PREF_PNM_PAM_ULK_STRICT_REG, _test) },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_BYTES_STRICT",
          run_builtin_loader_pnm_pam_large_header_bytes_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_BYTES_COMPAT",
          run_builtin_loader_pnm_pam_large_header_bytes_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_LINES_STRICT",
          run_builtin_loader_pnm_pam_large_header_lines_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_LINES_COMPAT",
          run_builtin_loader_pnm_pam_large_header_lines_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_BOUNDARY_STRICT",
          run_builtin_loader_pnm_pam_large_header_boundary_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_OVER_BYTES_STRICT",
          run_builtin_loader_pnm_pam_large_header_over_bytes_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_OVER_LINES_STRICT",
          run_builtin_loader_pnm_pam_large_header_over_lines_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_LARGE_HEADER_OVERLIMIT_COMPAT",
          run_builtin_loader_pnm_pam_large_header_overlimit_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_ASCII_TRUNCATED_STRICT",
          run_builtin_loader_pnm_ascii_truncated_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_ASCII_TRUNCATED_COMPAT",
          run_builtin_loader_pnm_ascii_truncated_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_SIGNATURE_GUARD",
          run_builtin_loader_pnm_signature_guard_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_KNOWN_KEY_EXTRA_TOKEN_REJECT",
          run_builtin_loader_pnm_pam_known_key_extra_token_reject_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_TUPLTYPE_DUPLICATE_LAST_WINS",
          run_builtin_loader_pnm_pam_tupletype_duplicate_last_wins_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_UNKNOWN_TUPLTYPE_FALLBACK_BOUNDARY",
          SIXEL_T0014_FN(SIXEL_T0014_PREF_PNM_PAM_UT_FALLBACK_BND, _test) },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_DUPLICATE_REQUIRED_KEY_STRICT",
          run_builtin_loader_pnm_pam_duplicate_required_key_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_DUPLICATE_REQUIRED_KEY_COMPAT",
          run_builtin_loader_pnm_pam_duplicate_required_key_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_TUPLTYPE_DUPLICATE_COMPAT_REGRESSION",
          run_builtin_loader_pnm_pam_tuple_dup_compat_regression_test },
        { "SIXEL_TEST_PNM_NUMERIC_TRAILING_DATA_STRICT",
          run_builtin_loader_pnm_trailing_data_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_TRAILING_DATA_COMPAT",
          run_builtin_loader_pnm_trailing_data_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_ASCII_TRAILING_COMMENT_STRICT",
          run_builtin_loader_pnm_ascii_trailing_comment_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_ENDHDR_TRAILING_TOKEN_STRICT",
          run_builtin_loader_pnm_pam_endhdr_trailing_token_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_ENDHDR_TRAILING_TOKEN_COMPAT",
          run_builtin_loader_pnm_pam_endhdr_trailing_token_compat_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_DUP_REQUIRED_REMAIN_STRICT",
          run_builtin_loader_pnm_pam_dup_required_keys_remain_strict_test },
        { "SIXEL_TEST_PNM_NUMERIC_PAM_DUP_REQUIRED_REMAIN_COMPAT",
          run_builtin_loader_pnm_pam_dup_required_keys_remain_compat_test }
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
            bmp_env_dispatch,
            sizeof(bmp_env_dispatch) / sizeof(bmp_env_dispatch[0])
        },
        {
            tga_env_dispatch,
            sizeof(tga_env_dispatch) / sizeof(tga_env_dispatch[0])
        },
        {
            pnm_env_dispatch,
            sizeof(pnm_env_dispatch) / sizeof(pnm_env_dispatch[0])
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
    /*
     * Keep optional expectation fields disabled unless this case sets them.
     */
    loader_component_case_spec_t psd_alpha_case = { 0 };

    dispatch_result = run_builtin_loader_env_dispatch_groups(
        env_dispatch_groups,
        sizeof(env_dispatch_groups) / sizeof(env_dispatch_groups[0]));
    if (dispatch_result >= 0) {
        return dispatch_result;
    }

    result = run_loader_component_case("builtin loader rgba8",
                                       RGBA_IMAGE_PATH,
                                       SIXEL_PIXELFORMAT_RGB888,
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
        SIXEL_PIXELFORMAT_RGB888,
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
        SIXEL_PIXELFORMAT_RGB888,
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

    result = run_builtin_loader_psdtools_2layer_parity_test();
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_psdtools_emoji_parity_test();
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_psdtools_transparentbg_parity_test();
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_psdtools_group_divider_parity_test();
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
