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
          run_builtin_loader_hdr_duplicate_header_metadata_numeric_test }
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
    static builtin_loader_env_dispatch_entry_t const psd_env_dispatch[] = {
        { "SIXEL_TEST_PSD_VALIDATE_DEFENSIVE",
          run_builtin_loader_psd_validate_defensive_test }
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
