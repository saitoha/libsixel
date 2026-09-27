/* SPDX-License-Identifier: MIT */
/* Compile actual CMS/loader bodies once under isolated test names. Only
 * dependency calls are replaced; no runtime fault option or copied helper
 * algorithm is used. Keep the namespace aligned with the included sources.
 * Normal public-loader values are tested against an independent oracle in
 * lab_linear_test_common.c, not against these paired execution controls. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/icc-apply.h"
#include "src/loader-libtiff.h"
#include "src/loader-common.h"
#include "src/chunk-factory.h"
#include "tests/data/color-numeric/colorsync-lab.h"
#include "tests/data/color-numeric/tiff-lab-linear.h"
#include "lab_linear_fault_test_common.h"
#if defined(__APPLE__) && HAVE_COREGRAPHICS
#include <ColorSync/ColorSync.h>
#endif

#define CLF_COUNT 521u
#define CLF_BYTES (CLF_COUNT * 3u * sizeof(float))
/* Capture the ordinary library before mapping test-copy entry points. */
static sixel_cms_engine_t (*clf_public_engine)(void) = sixel_cms_get_engine;
static int clf_arm;
static size_t clf_successes;
static size_t clf_rejections;
static size_t clf_alloc_failures;
static size_t clf_scratch_calls;
static size_t clf_apply_calls;
static size_t clf_poisoned;
static size_t clf_freed;
static float *clf_scratch;
static float clf_expected[CLF_COUNT * 3u];
static float clf_released[CLF_COUNT * 3u];
static int clf_prefix_match;
static int clf_suffix_untouched;
static int clf_prefix_changed;

static void *
clf_malloc(size_t n)
{
    void *p;
    size_t i;

    if (clf_arm && n == CLF_BYTES) {
        ++clf_scratch_calls;
        if (clf_arm == CMS_LAB_FAIL_ALLOCATION && !clf_alloc_failures) {
            ++clf_alloc_failures;
            return NULL;
        }
        p = malloc(n);
        clf_scratch = p;
        if (p != NULL) {
            for (i = 0u; i < CLF_COUNT * 3u; ++i) {
                clf_scratch[i] = -12345.0f;
            }
        }
        return p;
    }
    return malloc(n);
}

static void
clf_free(void *p)
{
    size_t i;

    if (p != NULL && p == (void *)clf_scratch) {
        ++clf_freed;
        memcpy(clf_released, p, sizeof(clf_released));
        clf_prefix_match = memcmp(p, clf_expected,
                                  256u * 3u * sizeof(float)) == 0;
        clf_prefix_changed = clf_scratch[0] != -12345.0f;
        clf_suffix_untouched = 1;
        for (i = 256u * 3u; i < CLF_COUNT * 3u; ++i) {
            if (clf_scratch[i] != -12345.0f) {
                clf_suffix_untouched = 0;
            }
        }
        clf_scratch = NULL;
    }
    free(p);
}

static int
clf_icc(double xyz[3], double const *src, size_t channels,
          sixel_icc_profile_t const *profile, unsigned int slot, int fallback)
{
    int ok;

    if (clf_arm) { ++clf_apply_calls; }
    if (clf_arm == CMS_LAB_FAIL_BUILTIN_LATE && clf_successes >= 256u) {
        ++clf_rejections;
        return 0;
    }
    ok = sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
        xyz, src, channels, profile, slot, fallback);
    if (clf_arm == CMS_LAB_FAIL_BUILTIN_LATE && ok) { ++clf_successes; }
    return ok;
}

#if defined(__APPLE__) && HAVE_COREGRAPHICS
static bool
clf_cs(ColorSyncTransformRef tr, size_t w, size_t h,
         void *dst, ColorSyncDataDepth dd, ColorSyncDataLayout dl, size_t ds,
         void const *src, ColorSyncDataDepth sd, ColorSyncDataLayout sl,
         size_t ss, CFDictionaryRef options)
{
    bool ok;

    if (clf_arm) { ++clf_apply_calls; }
    if (clf_arm == CMS_LAB_FAIL_COLORSYNC_LATE && clf_successes >= 256u) {
        ++clf_rejections;
        return false;
    }
    ok = ColorSyncTransformConvert(tr, w, h, dst, dd, dl, ds,
                                    src, sd, sl, ss, options);
    if ((clf_arm == CMS_LAB_FAIL_COLORSYNC_LATE ||
         clf_arm == CMS_LAB_FAIL_NAN ||
         clf_arm == CMS_LAB_FAIL_INFINITY) && ok) {
        clf_successes += w * h;
    }
    if ((clf_arm == CMS_LAB_FAIL_NAN || clf_arm == CMS_LAB_FAIL_INFINITY) &&
        ok && clf_successes == CLF_COUNT) {
        ((float *)dst)[w * h * 3u - 1u] =
            clf_arm == CMS_LAB_FAIL_NAN ? NAN : INFINITY;
        ++clf_poisoned;
    }
    return ok;
}
#endif
#define SIXEL_CMS_PROFILE_BACKEND_BUILTIN clf_local_0
#define SIXEL_CMS_PROFILE_BACKEND_COLORSYNC clf_local_1
#define SIXEL_CMS_PROFILE_BACKEND_LCMS2 clf_local_2
#define SIXEL_CMS_PROFILE_BACKEND_NONE clf_local_3
#define g_sixel_cms_engine clf_local_4
#define g_sixel_cms_engine_mutex clf_local_5
#define g_sixel_cms_engine_mutex_once clf_local_6
#define g_sixel_cms_engine_mutex_ready clf_local_7
#define g_sixel_cms_engine_once clf_local_8
#define g_sixel_loader_libtiff_vtbl clf_local_9
#define load_tiff clf_local_10
#define load_with_libtiff clf_local_11
#define loader_can_try_libtiff clf_local_12
#define sixel_cdef_clone clf_local_13
#define sixel_cdef_export_icc clf_local_13_export
#define sixel_cdef_finite clf_local_14
#define sixel_cdef_free clf_local_15
#define sixel_cdef_from_xyz clf_local_16
#define sixel_cdef_icc clf_local_17
#define sixel_cdef_matrix clf_local_18
#define sixel_cdef_name clf_local_19
#define sixel_cdef_output_ready clf_local_20
#define sixel_cdef_parse clf_local_21
#define sixel_cdef_pcs_scale clf_local_22
#define sixel_cdef_rgb clf_local_23
#define sixel_cdef_to_xyz clf_local_24
#define sixel_cms_allocate_profile clf_local_25
#define sixel_cms_apply_backend clf_local_26
#define sixel_cms_apply_matrix3 clf_local_27
#define sixel_cms_ascii_case_equal clf_local_28
#define sixel_cms_build_builtin_a2b_order clf_local_29
#define sixel_cms_build_intent_order clf_local_30
#define sixel_cms_build_rgb_to_xyz_matrix clf_local_31
#define sixel_cms_build_rgb_to_xyz_matrix_internal clf_local_32
#define sixel_cms_builtin_apply_cmyk_f32_intent clf_local_33
#define sixel_cms_builtin_apply_cmyk_u16_intent clf_local_34
#define sixel_cms_builtin_apply_cmyk_u8_intent clf_local_35
#define sixel_cms_builtin_apply_gray_u8_intent clf_local_36
#define sixel_cms_builtin_apply_rgb_f32_intent clf_local_37
#define sixel_cms_builtin_apply_rgb_u8_intent clf_local_38
#define sixel_cms_builtin_apply_src_to_xyz_intent clf_local_39
#define sixel_cms_builtin_apply_xyz_to_dst_intent clf_local_40
#define sixel_cms_builtin_convert_rgb8_to_rgbf32 clf_local_41
#define sixel_cms_builtin_kind_channel_count clf_local_42
#define sixel_cms_builtin_load_src_unit clf_local_43
#define sixel_cms_builtin_profile_can_convert clf_local_44
#define sixel_cms_builtin_profile_is_srgb clf_local_45
#define sixel_cms_builtin_store_dst_unit clf_local_46
#define sixel_cms_clamp_lab_ab clf_local_47
#define sixel_cms_clamp_unit clf_local_48
#define sixel_cms_close_profile clf_local_49
#define sixel_cms_colorsync_create_transform_one clf_local_50
#define sixel_cms_colorsync_intent_to_cfstring clf_local_51
#define sixel_cms_colorsync_lab_units clf_local_52
#define sixel_cms_colorsync_map_format clf_local_53
#define sixel_cms_colorsync_profile_channels clf_local_54
#define sixel_cms_colorsync_promote_source clf_local_55
#define sixel_cms_colorsync_source_matches clf_local_56
#define sixel_cms_convert_colorsync_linear clf_local_57
#define sixel_cms_convert_profile_to_cielab clf_local_58
#define sixel_cms_convert_profile_to_linearrgb clf_local_59
#define sixel_cms_convert_profile_to_srgb clf_local_60
#define sixel_cms_convert_rgbf32_gamma_to_linear clf_local_61
#define sixel_cms_convert_to_cielab_with_profile_bytes clf_local_62
#define sixel_cms_convert_to_linearrgb_with_profile_bytes clf_local_63
#define sixel_cms_convert_to_srgb_with_profile_bytes clf_local_64
#define sixel_cms_create_cielab_d50_profile clf_local_65
#define sixel_cms_create_linear_srgb_profile clf_local_66
#define sixel_cms_create_rgb_profile_from_gamma_chrm clf_local_67
#define sixel_cms_create_rgb_profile_from_gammas_chrm clf_local_68
#define sixel_cms_cicp_name clf_cicp_name
#define sixel_cms_create_named_rgb_profile clf_named_profile
#define sixel_cms_create_srgb_profile clf_local_69
#define sixel_cms_create_transform clf_local_70
#define sixel_cms_create_transform_builtin clf_local_71
#define sixel_cms_create_transform_colorsync clf_local_72
#define sixel_cms_create_transform_lcms2 clf_local_73
#define sixel_cms_decode_srgb_unit clf_local_74
#define sixel_cms_delete_transform clf_local_75
#define sixel_cms_do_transform clf_local_76
#define sixel_cms_do_transform_builtin clf_local_77
#define sixel_cms_do_transform_builtin_device_to_device clf_local_78
#define sixel_cms_do_transform_builtin_to_srgb clf_local_79
#define sixel_cms_encode_srgb_unit clf_local_80
#define sixel_cms_engine_from_string clf_local_81
#define sixel_cms_engine_lock clf_local_82
#define sixel_cms_engine_lock_init_once clf_local_83
#define sixel_cms_engine_to_string clf_local_84
#define sixel_cms_engine_unlock clf_local_85
#define sixel_cms_float_rgb clf_local_86
#define sixel_cms_float_source_xyz clf_local_87
#define sixel_cms_get_color_space clf_local_88
#define sixel_cms_get_engine clf_local_89
#define sixel_cms_intent_contains clf_local_90
#define sixel_cms_intent_name clf_local_91
#define sixel_cms_intent_to_a2b_slot clf_local_92
#define sixel_cms_lcms_input_units clf_local_93
#define sixel_cms_map_format_lcms clf_local_94
#define sixel_cms_matrix3_invert clf_local_95
#define sixel_cms_matrix3_invert_internal clf_local_96
#define sixel_cms_native_gamma clf_local_97
#define sixel_cms_open_profile_from_mem clf_local_98
#define sixel_cms_parse_icc_colorspace_signature clf_local_99
#define sixel_cms_prepare_linear_gray clf_local_100
#define sixel_cms_linear_unit_pixel clf_mixed_unit
#define sixel_cms_apply_linear_colorsync clf_mixed_apply
#define sixel_cms_prepare_linear_rgb clf_local_101
#define sixel_cms_profile clf_local_102
#define sixel_cms_profile_backend clf_local_103
#define sixel_cms_profile_backend_t clf_local_104
#define sixel_cms_profile_create_cielab_d50_builtin clf_local_105
#define sixel_cms_profile_create_cielab_d50_colorsync clf_local_106
#define sixel_cms_profile_create_cielab_d50_lcms2 clf_local_107
#define sixel_cms_profile_create_rgb_gamma_chrm_builtin clf_local_108
#define sixel_cms_profile_create_rgb_gamma_chrm_colorsync clf_local_109
#define sixel_cms_profile_create_rgb_gamma_chrm_lcms2 clf_local_110
#define sixel_cms_profile_create_srgb_builtin clf_local_111
#define sixel_cms_profile_create_srgb_colorsync clf_local_112
#define sixel_cms_profile_create_srgb_lcms2 clf_local_113
#define sixel_cms_profile_open_builtin clf_local_114
#define sixel_cms_profile_open_colorsync clf_local_115
#define sixel_cms_profile_open_lcms2 clf_local_116
#define sixel_cms_profile_t clf_local_117
#define sixel_cms_resolve_engine clf_local_118
#define sixel_cms_rgb_power_tag clf_local_119
#define sixel_cms_rgb_set_tag clf_local_120
#define sixel_cms_set_engine clf_local_121
#define sixel_cms_srgb_to_xyz_d65 clf_local_122
#define sixel_cms_srgb_unit_to_xyz_d50 clf_local_123
#define sixel_cms_trace_gamma clf_local_124
#define sixel_cms_trace_intent_order clf_local_125
#define sixel_cms_transform clf_local_126
#define sixel_cms_transform_engine clf_local_127
#define sixel_cms_transform_t clf_local_128
#define sixel_cms_xyz_d50_to_d65 clf_local_129
#define sixel_cms_xyz_d50_to_srgb_unit clf_local_130
#define sixel_cms_xyz_d65_to_d50 clf_local_131
#define sixel_cms_xyz_to_srgb_d65 clf_local_132
#define sixel_color_def clf_local_133
#define sixel_color_def_t clf_local_134
#define sixel_color_xform clf_local_135
#define sixel_color_xform_t clf_local_136
#define sixel_cxform_apply clf_local_137
#define sixel_cxform_free clf_local_138
#define sixel_cxform_new clf_local_139
#define sixel_loader_libtiff_component clf_local_140
#define sixel_loader_libtiff_component_t clf_local_141
#define sixel_loader_libtiff_load clf_local_142
#define sixel_loader_libtiff_name clf_local_143
#define sixel_loader_libtiff_new clf_local_144
#define sixel_loader_libtiff_placeholder clf_local_145
#define sixel_loader_libtiff_placeholder_function clf_local_146
#define sixel_loader_libtiff_predicate clf_local_147
#define sixel_loader_libtiff_ref clf_local_148
#define sixel_loader_libtiff_setopt clf_local_149
#define sixel_loader_libtiff_unref clf_local_150
#define tiff_clamp_lab_ab clf_local_151
#define tiff_clamp_unit clf_local_152
#define tiff_convert_embedded_icc_to_srgb clf_local_153
#define tiff_convert_embedded_icc_to_srgb_nolcms clf_local_155
#define tiff_decode_lab_u16_ab clf_local_157
#define tiff_decode_lab_u16_l clf_local_158
#define tiff_map_scanline_coordinates clf_local_159
#define tiff_memory_chunk clf_local_160
#define tiff_memory_chunk_t clf_local_161
#define tiff_memory_close clf_local_162
#define tiff_memory_map clf_local_163
#define tiff_memory_read clf_local_164
#define tiff_memory_seek clf_local_165
#define tiff_memory_size clf_local_166
#define tiff_memory_unmap clf_local_167
#define tiff_memory_write clf_local_168
#define tiff_normalize_lab_ab_sample clf_local_169
#define tiff_normalize_lab_l_sample clf_local_170
#define tiff_orientation_is_supported clf_local_171
#define tiff_photometric_allows_embedded_icc clf_local_172
#define tiff_try_load_high_precision clf_local_173
typedef struct sixel_cms_profile sixel_cms_profile_t;
typedef struct sixel_cms_transform sixel_cms_transform_t;
typedef struct sixel_color_def sixel_color_def_t;
typedef struct sixel_color_xform sixel_color_xform_t;
SIXEL_INTERNAL_API int
sixel_cms_engine_from_string(char const *text,
                             sixel_cms_engine_t *out_engine);
SIXEL_INTERNAL_API char const *
sixel_cms_engine_to_string(sixel_cms_engine_t engine);
SIXEL_INTERNAL_API void
sixel_cms_set_engine(sixel_cms_engine_t engine);
SIXEL_INTERNAL_API sixel_cms_engine_t
sixel_cms_get_engine(void);
SIXEL_INTERNAL_API int
sixel_cms_convert_profile_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  int pixelformat,
                                  sixel_cms_profile_t *src_profile);
SIXEL_INTERNAL_API int
sixel_cms_convert_to_srgb_with_profile_bytes(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_linear_srgb_profile(void);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_cielab_d50_profile(void);
SIXEL_INTERNAL_API int
sixel_cms_convert_profile_to_linearrgb(unsigned char *pixels,
                                       int width,
                                       int height,
                                       int pixelformat,
                                       sixel_cms_profile_t *src_profile);
SIXEL_INTERNAL_API int
sixel_cms_convert_to_linearrgb_with_profile_bytes(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);
SIXEL_INTERNAL_API int
sixel_cms_convert_colorsync_linear(float *pixels,
                                   int width,
                                   int height,
                                   int source_is_srgb,
                                   void const *profile_bytes,
                                   size_t profile_length);
SIXEL_INTERNAL_API int
sixel_cms_convert_profile_to_cielab(unsigned char *pixels,
                                    int width,
                                    int height,
                                    int pixelformat,
                                    sixel_cms_profile_t *src_profile);
SIXEL_INTERNAL_API int
sixel_cms_convert_to_cielab_with_profile_bytes(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length);
SIXEL_INTERNAL_API int
sixel_cms_convert_rgbf32_gamma_to_linear(float *pixels, size_t pixel_count);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_open_profile_from_mem(void const *data, size_t length);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_srgb_profile(void);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gamma_chrm(double file_gamma,
                                             double white_x,
                                             double white_y,
                                             double red_x,
                                             double red_y,
                                             double green_x,
                                             double green_y,
                                             double blue_x,
                                             double blue_y);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gammas_chrm(double gamma_r,
                                              double gamma_g,
                                              double gamma_b,
                                              double white_x,
                                              double white_y,
                                              double red_x,
                                              double red_y,
                                              double green_x,
                                              double green_y,
                                              double blue_x,
                                              double blue_y);
SIXEL_INTERNAL_API sixel_cms_color_space_t
sixel_cms_get_color_space(sixel_cms_profile_t const *profile);
SIXEL_INTERNAL_API void
sixel_cms_close_profile(sixel_cms_profile_t *profile);
SIXEL_INTERNAL_API sixel_cms_transform_t *
sixel_cms_create_transform(sixel_cms_profile_t const *src_profile,
                           sixel_cms_pixel_format_t src_format,
                           sixel_cms_profile_t const *dst_profile,
                           sixel_cms_pixel_format_t dst_format,
                           int flags);
SIXEL_INTERNAL_API int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src,
                       void *dst,
                       size_t pixel_count);
SIXEL_INTERNAL_API sixel_cms_engine_t
sixel_cms_transform_engine(sixel_cms_transform_t const *transform);
SIXEL_INTERNAL_API void
sixel_cms_delete_transform(sixel_cms_transform_t *transform);
SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_name(char const *name);
SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_rgb(double const xy[8], sixel_color_trc_t trc,
                double const gamma[3]);
SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_icc(void const *bytes, size_t length);
SIXEL_INTERNAL_API int
sixel_cdef_parse(char const *text, sixel_color_def_t **definition,
                  char const **icc_path, int *intent);
SIXEL_INTERNAL_API sixel_color_def_t *
sixel_cdef_clone(sixel_color_def_t const *definition);
SIXEL_INTERNAL_API void
sixel_cdef_free(sixel_color_def_t *definition);
SIXEL_INTERNAL_API int
sixel_cdef_to_xyz(sixel_color_def_t const *definition,
                   double const rgb[3], double xyz[3], int intent);
SIXEL_INTERNAL_API int
sixel_cdef_from_xyz(sixel_color_def_t const *definition,
                     double const xyz[3], double rgb[3], int intent);
SIXEL_INTERNAL_API int
sixel_cdef_output_ready(sixel_color_def_t const *definition,
                         int encoding_intent, int evaluation_intent);
SIXEL_INTERNAL_API sixel_color_xform_t *
sixel_cxform_new(sixel_color_def_t const *source,
                  sixel_color_def_t const *destination, int intent);
SIXEL_INTERNAL_API int
sixel_cxform_apply(sixel_color_xform_t const *transform,
                    double const source[3], double destination[3]);
SIXEL_INTERNAL_API void
sixel_cxform_free(sixel_color_xform_t *transform);
#define malloc clf_malloc
#define free clf_free
#define sixel_icc_apply_device_to_xyz_d50_with_a2b_slot clf_icc
#define ColorSyncTransformConvert clf_cs
SIXEL_INTERNAL_API char const *
sixel_cms_cicp_name(unsigned char const codes[4]);
SIXEL_INTERNAL_API sixel_cms_profile_t *
sixel_cms_create_named_rgb_profile(char const *name);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_cdef_export_icc(sixel_color_def_t const *definition,
                      unsigned char **bytes, size_t *length,
                      sixel_allocator_t *allocator);
#include "src/cms.c"
#undef malloc
#undef free
#undef sixel_icc_apply_device_to_xyz_d50_with_a2b_slot
#undef ColorSyncTransformConvert
#if HAVE_LIBTIFF
#define tiff_logluv_xyz_to_linear clf_logluv_xyz_to_linear
#define tiff_convert_samples_to_linear llbf_byte
#define tiff_try_load_integer_alpha llbf_alpha
#define tiff_read_separate_uint16 b1_namespace_6
#define tiff_publish_integer_alpha til_ns_14_0
#define tiff_integer_layout_t til_ns_14_1
#define tiff_scatter_integer_row til_ns_14_2
#define tiff_try_load_integer_layouts til_ns_14_3
#define tiff_lab8_ranges tlx_copy_10_ranges
#define tiff_lab8_to_linear tlx_copy_10_linear
#define tiff_convert_cmyk_to_linear lab_linearf_ink_linear
#include "src/loader-libtiff.c"
#undef tiff_convert_cmyk_to_linear
#undef tiff_lab8_to_linear
#undef tiff_lab8_ranges
#undef tiff_publish_integer_alpha
#undef tiff_integer_layout_t
#undef tiff_scatter_integer_row
#undef tiff_try_load_integer_layouts

#undef tiff_read_separate_uint16
#undef tiff_try_load_integer_alpha
#undef tiff_convert_samples_to_linear
#undef tiff_logluv_xyz_to_linear
#endif
#undef SIXEL_CMS_PROFILE_BACKEND_BUILTIN
#undef SIXEL_CMS_PROFILE_BACKEND_COLORSYNC
#undef SIXEL_CMS_PROFILE_BACKEND_LCMS2
#undef SIXEL_CMS_PROFILE_BACKEND_NONE
#undef g_sixel_cms_engine
#undef g_sixel_cms_engine_mutex
#undef g_sixel_cms_engine_mutex_once
#undef g_sixel_cms_engine_mutex_ready
#undef g_sixel_cms_engine_once
#undef g_sixel_loader_libtiff_vtbl
#undef load_tiff
#undef load_with_libtiff
#undef loader_can_try_libtiff
#undef sixel_cdef_clone
#undef sixel_cdef_export_icc
#undef sixel_cdef_finite
#undef sixel_cdef_free
#undef sixel_cdef_from_xyz
#undef sixel_cdef_icc
#undef sixel_cdef_matrix
#undef sixel_cdef_name
#undef sixel_cdef_output_ready
#undef sixel_cdef_parse
#undef sixel_cdef_pcs_scale
#undef sixel_cdef_rgb
#undef sixel_cdef_to_xyz
#undef sixel_cms_allocate_profile
#undef sixel_cms_apply_backend
#undef sixel_cms_apply_matrix3
#undef sixel_cms_ascii_case_equal
#undef sixel_cms_build_builtin_a2b_order
#undef sixel_cms_build_intent_order
#undef sixel_cms_build_rgb_to_xyz_matrix
#undef sixel_cms_build_rgb_to_xyz_matrix_internal
#undef sixel_cms_builtin_apply_cmyk_f32_intent
#undef sixel_cms_builtin_apply_cmyk_u16_intent
#undef sixel_cms_builtin_apply_cmyk_u8_intent
#undef sixel_cms_builtin_apply_gray_u8_intent
#undef sixel_cms_builtin_apply_rgb_f32_intent
#undef sixel_cms_builtin_apply_rgb_u8_intent
#undef sixel_cms_builtin_apply_src_to_xyz_intent
#undef sixel_cms_builtin_apply_xyz_to_dst_intent
#undef sixel_cms_builtin_convert_rgb8_to_rgbf32
#undef sixel_cms_builtin_kind_channel_count
#undef sixel_cms_builtin_load_src_unit
#undef sixel_cms_builtin_profile_can_convert
#undef sixel_cms_builtin_profile_is_srgb
#undef sixel_cms_builtin_store_dst_unit
#undef sixel_cms_clamp_lab_ab
#undef sixel_cms_clamp_unit
#undef sixel_cms_close_profile
#undef sixel_cms_colorsync_create_transform_one
#undef sixel_cms_colorsync_intent_to_cfstring
#undef sixel_cms_colorsync_lab_units
#undef sixel_cms_colorsync_map_format
#undef sixel_cms_colorsync_profile_channels
#undef sixel_cms_colorsync_promote_source
#undef sixel_cms_colorsync_source_matches
#undef sixel_cms_convert_colorsync_linear
#undef sixel_cms_convert_profile_to_cielab
#undef sixel_cms_convert_profile_to_linearrgb
#undef sixel_cms_convert_profile_to_srgb
#undef sixel_cms_convert_rgbf32_gamma_to_linear
#undef sixel_cms_convert_to_cielab_with_profile_bytes
#undef sixel_cms_convert_to_linearrgb_with_profile_bytes
#undef sixel_cms_convert_to_srgb_with_profile_bytes
#undef sixel_cms_create_cielab_d50_profile
#undef sixel_cms_create_linear_srgb_profile
#undef sixel_cms_create_rgb_profile_from_gamma_chrm
#undef sixel_cms_create_rgb_profile_from_gammas_chrm
#undef sixel_cms_cicp_name
#undef sixel_cms_create_named_rgb_profile
#undef sixel_cms_create_srgb_profile
#undef sixel_cms_create_transform
#undef sixel_cms_create_transform_builtin
#undef sixel_cms_create_transform_colorsync
#undef sixel_cms_create_transform_lcms2
#undef sixel_cms_decode_srgb_unit
#undef sixel_cms_delete_transform
#undef sixel_cms_do_transform
#undef sixel_cms_do_transform_builtin
#undef sixel_cms_do_transform_builtin_device_to_device
#undef sixel_cms_do_transform_builtin_to_srgb
#undef sixel_cms_encode_srgb_unit
#undef sixel_cms_engine_from_string
#undef sixel_cms_engine_lock
#undef sixel_cms_engine_lock_init_once
#undef sixel_cms_engine_to_string
#undef sixel_cms_engine_unlock
#undef sixel_cms_float_rgb
#undef sixel_cms_float_source_xyz
#undef sixel_cms_get_color_space
#undef sixel_cms_get_engine
#undef sixel_cms_intent_contains
#undef sixel_cms_intent_name
#undef sixel_cms_intent_to_a2b_slot
#undef sixel_cms_lcms_input_units
#undef sixel_cms_map_format_lcms
#undef sixel_cms_matrix3_invert
#undef sixel_cms_matrix3_invert_internal
#undef sixel_cms_native_gamma
#undef sixel_cms_open_profile_from_mem
#undef sixel_cms_parse_icc_colorspace_signature
#undef sixel_cms_prepare_linear_gray
#undef sixel_cms_linear_unit_pixel
#undef sixel_cms_apply_linear_colorsync
#undef sixel_cms_prepare_linear_rgb
#undef sixel_cms_profile
#undef sixel_cms_profile_backend
#undef sixel_cms_profile_backend_t
#undef sixel_cms_profile_create_cielab_d50_builtin
#undef sixel_cms_profile_create_cielab_d50_colorsync
#undef sixel_cms_profile_create_cielab_d50_lcms2
#undef sixel_cms_profile_create_rgb_gamma_chrm_builtin
#undef sixel_cms_profile_create_rgb_gamma_chrm_colorsync
#undef sixel_cms_profile_create_rgb_gamma_chrm_lcms2
#undef sixel_cms_profile_create_srgb_builtin
#undef sixel_cms_profile_create_srgb_colorsync
#undef sixel_cms_profile_create_srgb_lcms2
#undef sixel_cms_profile_open_builtin
#undef sixel_cms_profile_open_colorsync
#undef sixel_cms_profile_open_lcms2
#undef sixel_cms_profile_t
#undef sixel_cms_resolve_engine
#undef sixel_cms_rgb_power_tag
#undef sixel_cms_rgb_set_tag
#undef sixel_cms_set_engine
#undef sixel_cms_srgb_to_xyz_d65
#undef sixel_cms_srgb_unit_to_xyz_d50
#undef sixel_cms_trace_gamma
#undef sixel_cms_trace_intent_order
#undef sixel_cms_transform
#undef sixel_cms_transform_engine
#undef sixel_cms_transform_t
#undef sixel_cms_xyz_d50_to_d65
#undef sixel_cms_xyz_d50_to_srgb_unit
#undef sixel_cms_xyz_d65_to_d50
#undef sixel_cms_xyz_to_srgb_d65
#undef sixel_color_def
#undef sixel_color_def_t
#undef sixel_color_xform
#undef sixel_color_xform_t
#undef sixel_cxform_apply
#undef sixel_cxform_free
#undef sixel_cxform_new
#undef sixel_loader_libtiff_component
#undef sixel_loader_libtiff_component_t
#undef sixel_loader_libtiff_load
#undef sixel_loader_libtiff_name
#undef sixel_loader_libtiff_new
#undef sixel_loader_libtiff_placeholder
#undef sixel_loader_libtiff_placeholder_function
#undef sixel_loader_libtiff_predicate
#undef sixel_loader_libtiff_ref
#undef sixel_loader_libtiff_setopt
#undef sixel_loader_libtiff_unref
#undef tiff_clamp_lab_ab
#undef tiff_clamp_unit
#undef tiff_convert_embedded_icc_to_srgb
#undef tiff_convert_embedded_icc_to_srgb_nolcms
#undef tiff_decode_lab_u16_ab
#undef tiff_decode_lab_u16_l
#undef tiff_map_scanline_coordinates
#undef tiff_memory_chunk
#undef tiff_memory_chunk_t
#undef tiff_memory_close
#undef tiff_memory_map
#undef tiff_memory_read
#undef tiff_memory_seek
#undef tiff_memory_size
#undef tiff_memory_unmap
#undef tiff_memory_write
#undef tiff_normalize_lab_ab_sample
#undef tiff_normalize_lab_l_sample
#undef tiff_orientation_is_supported
#undef tiff_photometric_allows_embedded_icc
#undef tiff_try_load_high_precision
static void
clf_reset(int mode)
{
    clf_arm = mode;
    clf_successes = 0u;
    clf_rejections = 0u;
    clf_alloc_failures = 0u;
    clf_scratch_calls = 0u;
    clf_apply_calls = 0u;
    clf_poisoned = 0u;
    clf_freed = 0u;
    clf_scratch = NULL;
    clf_prefix_match = 0;
    clf_prefix_changed = 0;
    clf_suffix_untouched = 0;
    memset(clf_released, 0, sizeof(clf_released));
}

static int
clf_fault_valid(int mode)
{
    int ok;

    ok = clf_scratch_calls == 1u && clf_scratch == NULL;
    if (mode == CMS_LAB_FAIL_ALLOCATION) {
        ok = ok && clf_alloc_failures == 1u && clf_freed == 0u &&
             clf_apply_calls == 0u;
    } else {
        ok = ok && clf_freed == 1u && clf_prefix_match && clf_prefix_changed;
        if (mode == CMS_LAB_FAIL_BUILTIN_LATE ||
            mode == CMS_LAB_FAIL_COLORSYNC_LATE) {
            ok = ok && clf_successes == 256u && clf_rejections > 0u &&
                 clf_suffix_untouched;
        } else {
            ok = ok && clf_successes == CLF_COUNT && clf_poisoned == 1u;
        }
    }
    fprintf(stderr, "TLINFAULT|mode=%d|qualified=%d|prefix=%d|changed=%d|"
        "tail=%d|success=%zu|rejects=%zu|scratch=%zu|denied=%zu|poison=%zu\n",
        mode, ok, clf_prefix_match, clf_prefix_changed, clf_suffix_untouched,
        clf_successes, clf_rejections, clf_scratch_calls,
        clf_alloc_failures, clf_poisoned);
    return ok;
}

static int
clf_normal_valid(void const *published)
{
    return clf_scratch_calls == 1u && clf_freed == 1u &&
           clf_scratch == NULL && clf_alloc_failures == 0u &&
           memcmp(published, clf_released, CLF_BYTES) == 0;
}

static int
clf_guards(unsigned char const *arena)
{
    size_t i;

    for (i = 0u; i < 32u; ++i) {
        if (arena[i] != 0xa5 || arena[32u + CLF_BYTES + i] != 0xa5) {
            return 0;
        }
    }
    return 1;
}

static sixel_cms_engine_t
clf_engine(int mode)
{
    return mode == CMS_LAB_FAIL_BUILTIN_LATE ||
           mode == CMS_LAB_FAIL_ALLOCATION
         ? SIXEL_CMS_ENGINE_BUILTIN : SIXEL_CMS_ENGINE_COLORSYNC;
}

int
cms_lab_failure_probe(int mode)
{
    sixel_cms_engine_t previous;
    sixel_cms_engine_t ordinary;
    clf_local_117 *profile;
    unsigned char saved[CLF_BYTES + 64u];
    unsigned char *arena;
    int applied;
    int normal;
    int recovered;
    int unchanged;
    int qualified;
    int ok;

    previous = clf_local_89();
    ordinary = clf_public_engine();
    clf_local_121(clf_engine(mode));
    if (clf_local_89() != clf_engine(mode)) {
        clf_local_121(previous);
        return 77;
    }
    clf_reset(0);
    memset(clf_expected, 0, sizeof(clf_expected));
    arena = malloc(sizeof(saved));
    profile = clf_local_98(cs_lab_icc, sizeof(cs_lab_icc));
    ok = 0;
    if (arena == NULL || profile == NULL) { goto cleanup; }
    /* The caller's profile is open before arming. A normal observed pass
     * identifies the unique result allocation by its complete output. */
    if (sizeof(*profile) == CLF_BYTES) { goto cleanup; }
    memset(saved, 0xa5, sizeof(saved));
    memcpy(saved + 32u, tll_source[0], CLF_BYTES);
    memcpy(arena, saved, sizeof(saved));
    clf_reset(6);
    applied = clf_local_59(arena + 32u, 521, 1,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, profile);
    clf_arm = 0;
    normal = clf_normal_valid(arena + 32u) && clf_guards(arena) && applied;
    memcpy(clf_expected, arena + 32u, CLF_BYTES);

    memcpy(arena, saved, sizeof(saved));
    clf_reset(mode);
    applied = clf_local_59(arena + 32u, 521, 1,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, profile);
    clf_arm = 0;
    /* Evaluate qualification and input preservation independently, so a
     * mutation cannot hide corruption behind a failed seam observation. */
    qualified = clf_fault_valid(mode);
    unchanged = memcmp(arena, saved, sizeof(saved)) == 0;
    fprintf(stderr, "TLINHELPER|mode=%d|applied=%d|input-guards=%d\n",
        mode, applied, unchanged);
    ok = normal && qualified && !applied && unchanged;

    memcpy(arena, saved, sizeof(saved));
    clf_reset(6);
    applied = clf_local_59(arena + 32u, 521, 1,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, profile);
    clf_arm = 0;
    recovered = clf_normal_valid(arena + 32u) && clf_guards(arena) &&
        applied && memcmp(arena + 32u, clf_expected, CLF_BYTES) == 0;
    fprintf(stderr, "TLINHELPER|normal=%d|recovered=%d\n", normal, recovered);
    ok = ok && recovered;
cleanup:
    clf_reset(0);
    memset(clf_expected, 0, sizeof(clf_expected));
    clf_local_49(profile);
    free(arena);
    clf_local_121(previous);
    ok = ok && clf_public_engine() == ordinary;
    return ok ? 0 : 1;
}

#if HAVE_LIBTIFF
typedef struct clf_frame_state {
    float pixels[CLF_COUNT * 3u];
    int format;
    int colorspace;
    int calls;
} clf_frame_state_t;

static SIXELSTATUS
clf_capture(sixel_frame_t *frame, void *context)
{
    clf_frame_state_t *state;

    state = context;
    ++state->calls;
    if (frame->width != 521 || frame->height != 1 ||
        frame->ncolors != -1 || frame->transparent != -1 ||
        frame->transparent_mask != NULL ||
        frame->transparent_mask_size != 0u ||
        frame->alpha_zero_is_transparent != 0) {
        fprintf(stderr, "TLINLOADER|metadata-failed\n");
        return SIXEL_BAD_INPUT;
    }
    state->format = frame->pixelformat;
    state->colorspace = frame->colorspace;
    memcpy(state->pixels, sixel_frame_get_pixels(frame), CLF_BYTES);
    return SIXEL_OK;
}

static int
clf_load(sixel_loader_component_t *component, sixel_chunk_t *chunk,
           int cms, clf_frame_state_t *state)
{
    SIXELSTATUS status;

    memset(state, 0, sizeof(*state));
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &cms);
    if (SIXEL_FAILED(status)) { return 0; }
    /* Use the actual included component entry, including timeline cleanup. */
    status = component->vtbl->load(component, chunk, clf_capture, state);
    return SIXEL_SUCCEEDED(status) && state->calls == 1;
}
#endif

int
tiff_lab_failure_probe(int argc, char **argv, int mode)
{
#if HAVE_LIBTIFF
    sixel_cms_engine_t previous;
    sixel_cms_engine_t ordinary;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk;
    sixel_allocator_t *allocator;
    clf_frame_state_t fallback;
    clf_frame_state_t frame;
    void *object;
    SIXELSTATUS status;
    int fallback_ok;
    int normal;
    int loaded;
    int qualified;
    int pixels_same;
    int metadata;
    int recovered;
    int ok;

    if (argc != 2) { return 2; }
    previous = clf_local_89();
    ordinary = clf_public_engine();
    clf_local_121(clf_engine(mode));
    if (clf_local_89() != clf_engine(mode)) {
        clf_local_121(previous);
        return 77;
    }
    clf_reset(0);
    memset(clf_expected, 0, sizeof(clf_expected));
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    ok = 0;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, argv[1], 0,
                                             NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = clf_local_144(allocator, &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    fallback_ok = clf_load(component, chunk, 0, &fallback);
    fallback_ok = fallback_ok &&
        fallback.format == SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        fallback.colorspace == SIXEL_COLORSPACE_GAMMA;

    clf_reset(6);
    loaded = clf_load(component, chunk, 1, &frame);
    clf_arm = 0;
    normal = clf_normal_valid(frame.pixels) && loaded &&
        frame.format == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
        frame.colorspace == SIXEL_COLORSPACE_LINEAR;
    memcpy(clf_expected, frame.pixels, CLF_BYTES);

    clf_reset(mode);
    loaded = clf_load(component, chunk, 1, &frame);
    clf_arm = 0;
    qualified = clf_fault_valid(mode);
    pixels_same = memcmp(fallback.pixels, frame.pixels, CLF_BYTES) == 0;
    metadata = frame.format == fallback.format &&
               frame.colorspace == fallback.colorspace;
    fprintf(stderr, "TLINLOADER|mode=%d|loaded=%d|pixels=%d|metadata=%d\n",
        mode, loaded, pixels_same, metadata);
    ok = fallback_ok && normal && qualified && loaded &&
         pixels_same && metadata;

    clf_reset(6);
    loaded = clf_load(component, chunk, 1, &frame);
    clf_arm = 0;
    recovered = clf_normal_valid(frame.pixels) && loaded &&
        frame.format == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
        frame.colorspace == SIXEL_COLORSPACE_LINEAR &&
        memcmp(frame.pixels, clf_expected, CLF_BYTES) == 0;
    fprintf(stderr, "TLINLOADER|normal=%d|recovered=%d\n", normal, recovered);
    ok = ok && recovered;
cleanup:
    clf_reset(0);
    memset(clf_expected, 0, sizeof(clf_expected));
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    clf_local_121(previous);
    ok = ok && clf_public_engine() == ordinary;
    return ok ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    (void)mode;
    return 77;
#endif
}

#undef CLF_COUNT
#undef CLF_BYTES
