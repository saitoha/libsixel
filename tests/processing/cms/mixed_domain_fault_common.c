/* SPDX-License-Identifier: MIT */
/* Include the actual CMS body under isolated test names. Faults intercept
 * allocation and native dependencies, never a copied grouping algorithm. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/icc-apply.h"
#include "src/compat_stub.h"
#include "mixed_domain_common.h"
#if defined(__APPLE__) && HAVE_COREGRAPHICS
#include <ColorSync/ColorSync.h>

static int cmf_arm;
static int cmf_fault;
static int cmf_hits;
static int cmf_applies;
static int cmf_after_fault;
static int cmf_allocations;
static int cmf_qualified;
static size_t cmf_size;
static size_t cmf_reorder_size;
static void *cmf_live[64];

static void *
cmf_malloc(size_t size)
{
    void *p;
    size_t i;

    if (cmf_arm && size == cmf_size) {
        ++cmf_allocations;
        if ((cmf_fault == 0 && cmf_allocations == 2) ||
            (cmf_fault == 1 && cmf_allocations == 3)) {
            ++cmf_hits;
            return NULL;
        }
    }
    /* The second permutation is allocated only after a real first apply.
     * No output may escape if this later preparation cannot be completed. */
    if (cmf_arm && cmf_fault == 7 && cmf_applies == 1 &&
        size == cmf_reorder_size) {
        ++cmf_hits;
        return NULL;
    }
    p = malloc(size);
    if (cmf_arm && p != NULL) {
        for (i = 0u; i < 64u; ++i) {
            if (cmf_live[i] == NULL) { cmf_live[i] = p; break; }
        }
        if (i == 64u) { abort(); }
    }
    return p;
}

static void
cmf_free(void *p)
{
    size_t i;

    for (i = 0u; i < 64u; ++i) {
        if (cmf_live[i] == p) { cmf_live[i] = NULL; }
    }
    free(p);
}

static ColorSyncTransformRef
cmf_create(CFArrayRef profiles, CFDictionaryRef options)
{
    if (cmf_arm && cmf_hits) { ++cmf_after_fault; }
    if (cmf_arm && ((cmf_fault == 2 && cmf_applies == 0) ||
                   (cmf_fault == 3 && cmf_applies == 1))) {
        ++cmf_hits;
        return NULL;
    }
    return ColorSyncTransformCreate(profiles, options);
}

static bool
cmf_apply(ColorSyncTransformRef tr, size_t w, size_t h,
         void *dst, ColorSyncDataDepth dd, ColorSyncDataLayout dl, size_t ds,
         void const *src, ColorSyncDataDepth sd, ColorSyncDataLayout sl,
         size_t ss, CFDictionaryRef options)
{
    float *out;
    size_t i;
    size_t count;
    int changed;
    int untouched;
    bool ok;

    if (cmf_arm) { ++cmf_applies; }
    count = w * h;
    out = dst;
    if (cmf_arm && cmf_fault == 4 && cmf_applies == 2) {
        if (h != 1u || count < 4u) { return false; }
        for (i = 0u; i < count * 3u; ++i) { out[i] = -12345.0f; }
        ok = ColorSyncTransformConvert(tr, 2u, 1u, dst, dd, dl, ds,
                                        src, sd, sl, ss, options);
        changed = 0;
        untouched = 1;
        for (i = 0u; i < count * 3u; ++i) {
            if (i < 6u) { changed |= out[i] != -12345.0f; }
            else { untouched &= out[i] == -12345.0f; }
        }
        cmf_qualified = ok && changed && untouched;
        ++cmf_hits;
        return false;
    }
    ok = ColorSyncTransformConvert(tr, w, h, dst, dd, dl, ds,
                                    src, sd, sl, ss, options);
    if (cmf_arm && ok && ((cmf_fault == 5 && cmf_applies == 2) ||
                        (cmf_fault == 6 && cmf_applies == 1))) {
        out[count * 3u - 1u] = NAN;
        cmf_qualified = 1;
        ++cmf_hits;
    }
    return ok;
}
#define SIXEL_CMS_PROFILE_BACKEND_BUILTIN cmf_local_0
#define SIXEL_CMS_PROFILE_BACKEND_COLORSYNC cmf_local_1
#define SIXEL_CMS_PROFILE_BACKEND_LCMS2 cmf_local_2
#define SIXEL_CMS_PROFILE_BACKEND_NONE cmf_local_3
#define g_sixel_cms_engine cmf_local_4
#define g_sixel_cms_engine_mutex cmf_local_5
#define g_sixel_cms_engine_mutex_once cmf_local_6
#define g_sixel_cms_engine_mutex_ready cmf_local_7
#define g_sixel_cms_engine_once cmf_local_8
#define sixel_cdef_clone cmf_local_13
#define sixel_cdef_export_icc cmf_local_13_export
#define sixel_cdef_finite cmf_local_14
#define sixel_cdef_free cmf_local_15
#define sixel_cdef_from_xyz cmf_local_16
#define sixel_cdef_icc cmf_local_17
#define sixel_cdef_matrix cmf_local_18
#define sixel_cdef_name cmf_local_19
#define sixel_cdef_output_ready cmf_local_20
#define sixel_cdef_parse cmf_local_21
#define sixel_cdef_pcs_scale cmf_local_22
#define sixel_cdef_rgb cmf_local_23
#define sixel_cdef_to_xyz cmf_local_24
#define sixel_cms_allocate_profile cmf_local_25
#define sixel_cms_apply_backend cmf_local_26
#define sixel_cms_apply_matrix3 cmf_local_27
#define sixel_cms_ascii_case_equal cmf_local_28
#define sixel_cms_build_builtin_a2b_order cmf_local_29
#define sixel_cms_build_intent_order cmf_local_30
#define sixel_cms_build_rgb_to_xyz_matrix cmf_local_31
#define sixel_cms_build_rgb_to_xyz_matrix_internal cmf_local_32
#define sixel_cms_builtin_apply_cmyk_f32_intent cmf_local_33
#define sixel_cms_builtin_apply_cmyk_u16_intent cmf_local_34
#define sixel_cms_builtin_apply_cmyk_u8_intent cmf_local_35
#define sixel_cms_builtin_apply_gray_u8_intent cmf_local_36
#define sixel_cms_builtin_apply_rgb_f32_intent cmf_local_37
#define sixel_cms_builtin_apply_rgb_u8_intent cmf_local_38
#define sixel_cms_builtin_apply_src_to_xyz_intent cmf_local_39
#define sixel_cms_builtin_apply_xyz_to_dst_intent cmf_local_40
#define sixel_cms_builtin_convert_rgb8_to_rgbf32 cmf_local_41
#define sixel_cms_builtin_kind_channel_count cmf_local_42
#define sixel_cms_builtin_load_src_unit cmf_local_43
#define sixel_cms_builtin_profile_can_convert cmf_local_44
#define sixel_cms_builtin_profile_is_srgb cmf_local_45
#define sixel_cms_builtin_store_dst_unit cmf_local_46
#define sixel_cms_clamp_lab_ab cmf_local_47
#define sixel_cms_clamp_unit cmf_local_48
#define sixel_cms_close_profile cmf_local_49
#define sixel_cms_colorsync_create_transform_one cmf_local_50
#define sixel_cms_colorsync_intent_to_cfstring cmf_local_51
#define sixel_cms_colorsync_lab_units cmf_local_52
#define sixel_cms_colorsync_map_format cmf_local_53
#define sixel_cms_colorsync_profile_channels cmf_local_54
#define sixel_cms_colorsync_promote_source cmf_local_55
#define sixel_cms_colorsync_source_matches cmf_local_56
#define sixel_cms_convert_colorsync_linear cmf_local_57
#define sixel_cms_convert_profile_to_cielab cmf_local_58
#define sixel_cms_convert_profile_to_linearrgb cmf_local_59
#define sixel_cms_convert_profile_to_srgb cmf_local_60
#define sixel_cms_convert_rgbf32_gamma_to_linear cmf_local_61
#define sixel_cms_convert_to_cielab_with_profile_bytes cmf_local_62
#define sixel_cms_convert_to_linearrgb_with_profile_bytes cmf_local_63
#define sixel_cms_convert_to_srgb_with_profile_bytes cmf_local_64
#define sixel_cms_create_cielab_d50_profile cmf_local_65
#define sixel_cms_create_linear_srgb_profile cmf_local_66
#define sixel_cms_create_rgb_profile_from_gamma_chrm cmf_local_67
#define sixel_cms_create_rgb_profile_from_gammas_chrm cmf_local_68
#define sixel_cms_create_srgb_profile cmf_local_69
#define sixel_cms_create_transform cmf_local_70
#define sixel_cms_create_transform_builtin cmf_local_71
#define sixel_cms_create_transform_colorsync cmf_local_72
#define sixel_cms_create_transform_lcms2 cmf_local_73
#define sixel_cms_decode_srgb_unit cmf_local_74
#define sixel_cms_delete_transform cmf_local_75
#define sixel_cms_do_transform cmf_local_76
#define sixel_cms_do_transform_builtin cmf_local_77
#define sixel_cms_do_transform_builtin_device_to_device cmf_local_78
#define sixel_cms_do_transform_builtin_to_srgb cmf_local_79
#define sixel_cms_encode_srgb_unit cmf_local_80
#define sixel_cms_engine_from_string cmf_local_81
#define sixel_cms_engine_lock cmf_local_82
#define sixel_cms_engine_lock_init_once cmf_local_83
#define sixel_cms_engine_to_string cmf_local_84
#define sixel_cms_engine_unlock cmf_local_85
#define sixel_cms_float_rgb cmf_local_86
#define sixel_cms_float_source_xyz cmf_local_87
#define sixel_cms_get_color_space cmf_local_88
#define sixel_cms_get_engine cmf_local_89
#define sixel_cms_intent_contains cmf_local_90
#define sixel_cms_intent_name cmf_local_91
#define sixel_cms_intent_to_a2b_slot cmf_local_92
#define sixel_cms_lcms_input_units cmf_local_93
#define sixel_cms_map_format_lcms cmf_local_94
#define sixel_cms_matrix3_invert cmf_local_95
#define sixel_cms_matrix3_invert_internal cmf_local_96
#define sixel_cms_native_gamma cmf_local_97
#define sixel_cms_open_profile_from_mem cmf_local_98
#define sixel_cms_parse_icc_colorspace_signature cmf_local_99
#define sixel_cms_prepare_linear_gray cmf_local_100
#define sixel_cms_linear_unit_pixel cmf_mixed_unit
#define sixel_cms_apply_linear_colorsync cmf_mixed_apply
#define sixel_cms_prepare_linear_rgb cmf_local_101
#define sixel_cms_profile cmf_local_102
#define sixel_cms_profile_backend cmf_local_103
#define sixel_cms_profile_backend_t cmf_local_104
#define sixel_cms_profile_create_cielab_d50_builtin cmf_local_105
#define sixel_cms_profile_create_cielab_d50_colorsync cmf_local_106
#define sixel_cms_profile_create_cielab_d50_lcms2 cmf_local_107
#define sixel_cms_profile_create_rgb_gamma_chrm_builtin cmf_local_108
#define sixel_cms_profile_create_rgb_gamma_chrm_colorsync cmf_local_109
#define sixel_cms_profile_create_rgb_gamma_chrm_lcms2 cmf_local_110
#define sixel_cms_profile_create_srgb_builtin cmf_local_111
#define sixel_cms_profile_create_srgb_colorsync cmf_local_112
#define sixel_cms_profile_create_srgb_lcms2 cmf_local_113
#define sixel_cms_profile_open_builtin cmf_local_114
#define sixel_cms_profile_open_colorsync cmf_local_115
#define sixel_cms_profile_open_lcms2 cmf_local_116
#define sixel_cms_profile_t cmf_local_117
#define sixel_cms_resolve_engine cmf_local_118
#define sixel_cms_rgb_power_tag cmf_local_119
#define sixel_cms_rgb_set_tag cmf_local_120
#define sixel_cms_set_engine cmf_local_121
#define sixel_cms_srgb_to_xyz_d65 cmf_local_122
#define sixel_cms_srgb_unit_to_xyz_d50 cmf_local_123
#define sixel_cms_trace_gamma cmf_local_124
#define sixel_cms_trace_intent_order cmf_local_125
#define sixel_cms_transform cmf_local_126
#define sixel_cms_transform_engine cmf_local_127
#define sixel_cms_transform_t cmf_local_128
#define sixel_cms_xyz_d50_to_d65 cmf_local_129
#define sixel_cms_xyz_d50_to_srgb_unit cmf_local_130
#define sixel_cms_xyz_d65_to_d50 cmf_local_131
#define sixel_cms_xyz_to_srgb_d65 cmf_local_132
#define sixel_color_def cmf_local_133
#define sixel_color_def_t cmf_local_134
#define sixel_color_xform cmf_local_135
#define sixel_color_xform_t cmf_local_136
#define sixel_cxform_apply cmf_local_137
#define sixel_cxform_free cmf_local_138
#define sixel_cxform_new cmf_local_139
#define sixel_loader_libtiff_component cmf_local_140
#define sixel_loader_libtiff_component_t cmf_local_141
#define sixel_loader_libtiff_load cmf_local_142
#define sixel_loader_libtiff_name cmf_local_143
#define sixel_loader_libtiff_new cmf_local_144
#define sixel_loader_libtiff_placeholder cmf_local_145
#define sixel_loader_libtiff_placeholder_function cmf_local_146
#define sixel_loader_libtiff_predicate cmf_local_147
#define sixel_loader_libtiff_ref cmf_local_148
#define sixel_loader_libtiff_setopt cmf_local_149
#define sixel_loader_libtiff_unref cmf_local_150
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
#define malloc cmf_malloc
#define free cmf_free
#define ColorSyncTransformCreate cmf_create
#define ColorSyncTransformConvert cmf_apply
#include "src/cms.c"
#undef malloc
#undef free
#undef ColorSyncTransformCreate
#undef ColorSyncTransformConvert
#undef SIXEL_CMS_PROFILE_BACKEND_BUILTIN
#undef SIXEL_CMS_PROFILE_BACKEND_COLORSYNC
#undef SIXEL_CMS_PROFILE_BACKEND_LCMS2
#undef SIXEL_CMS_PROFILE_BACKEND_NONE
#undef g_sixel_cms_engine
#undef g_sixel_cms_engine_mutex
#undef g_sixel_cms_engine_mutex_once
#undef g_sixel_cms_engine_mutex_ready
#undef g_sixel_cms_engine_once
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
#endif

int
cms_mixed_failure(int observation)
{
#if defined(__APPLE__) && HAVE_COREGRAPHICS
    static int const gray[8] = {1, 0, 1, 0, 1, 0, 0, 0};
    static int const strict[8] = {0, 0, 0, 1, 0, 1, 0, 0};
    static int const applies[8] = {0, 0, 0, 1, 2, 2, 1, 1};
    cms_mixed_fixture_t f;
    cmf_local_117 *profile;
    CFDataRef before;
    CFDataRef after;
    unsigned char original[1024];
    float expected[CMS_MIXED_MAX * 3u];
    float guarded[CMS_MIXED_MAX * 3u + 2u];
    size_t bytes;
    size_t i;
    int result;
    int ok;

    if (observation < 0 || observation > 7) { return 1; }
    cms_mixed_fixture(gray[observation] ? 0 : 4, &f);
    memcpy(original, f.icc, f.length);
    bytes = f.count * 3u * sizeof(float);
    cmf_arm = 0;
    cmf_fault = observation;
    cmf_hits = cmf_applies = cmf_allocations = 0;
    cmf_after_fault = cmf_qualified = 0;
    memset(cmf_live, 0, sizeof(cmf_live));
    cmf_size = f.count * (gray[observation] ? 1u : 3u) * sizeof(float);
    cmf_reorder_size = (f.count - f.normalized) * 3u * sizeof(float);
    cmf_local_121(SIXEL_CMS_ENGINE_COLORSYNC);
    ok = sixel_compat_setenv(
        "SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA", "0") == 0;
    profile = cmf_local_98(f.icc, f.length);
    if (profile == NULL) { return 1; }
    before = ColorSyncProfileCopyData(profile->colorsync_handle, NULL);
    memcpy(expected, f.input, bytes);
    ok = ok && cmf_local_59(
        (unsigned char *)expected, (int)f.count, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    memcpy(guarded + 1, f.input, bytes);
    guarded[0] = guarded[f.count * 3u + 1u] = 12345.0f;
    cmf_arm = 1;
    result = strict[observation]
        ? cmf_local_57(guarded + 1, (int)f.count,
            1, 0, f.icc, f.length)
        : cmf_local_59((unsigned char *)(guarded + 1),
            (int)f.count, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    cmf_arm = 0;
    ok = ok && !result && cmf_hits > 0 && cmf_applies == applies[observation]
        && (observation < 4 || observation == 7 || cmf_qualified)
        && (observation != 6 || cmf_after_fault == 0)
        && guarded[0] == 12345.0f && guarded[f.count * 3u + 1u] == 12345.0f
        && memcmp(guarded + 1, f.input, bytes) == 0;
    for (i = 0u; i < 64u; ++i) { ok = ok && cmf_live[i] == NULL; }
    /* Recovery uses the same borrowed source, after the genuine partial
     * native write or successful nonfinite output was discarded. */
    ok = ok && cmf_local_59(
        (unsigned char *)(guarded + 1), (int)f.count, 1,
        SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    for (i = 0u; ok && i < f.count * 3u; ++i) {
        ok = isfinite(guarded[i + 1u]) && guarded[i + 1u] == expected[i];
    }
    after = ColorSyncProfileCopyData(profile->colorsync_handle, NULL);
    ok = ok && before != NULL && after != NULL && CFEqual(before, after)
        && memcmp(original, f.icc, f.length) == 0;
    fprintf(stderr, "MDFAULT|case=%d|hits=%d|applies=%d|qualified=%d|ok=%d\n",
        observation, cmf_hits, cmf_applies, cmf_qualified, ok);
    if (before != NULL) { CFRelease(before); }
    if (after != NULL) { CFRelease(after); }
    cmf_local_49(profile);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA", "");
    return ok ? 0 : 1;
#else
    (void)observation;
    return 77;
#endif
}
