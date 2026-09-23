/* SPDX-License-Identifier: MIT */
/* Compile the actual CMS and TIFF adapter under isolated names. Faults only
 * replace dependency calls; the publication algorithm is not copied. The
 * complete scratch is observed at release, after a real changed segment. */
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/icc-apply.h"
#include "src/loader-libtiff.h"
#include "src/loader-common.h"
#include "src/chunk-factory.h"
#include "tests/data/color-numeric/tiff-managed.h"
#include "tiff_managed_test_common.h"
#if HAVE_LIBTIFF
#include <tiffio.h>
#if defined(__APPLE__) && HAVE_COREGRAPHICS
#include <ColorSync/ColorSync.h>
#endif
#if HAVE_LCMS2
#include <lcms2.h>
#endif
static int mf_mode;
static int mf_phase;
static int mf_fixture;
static int mf_engine;
static size_t mf_n;
static size_t mf_success;
static size_t mf_curve_calls;
static size_t mf_reject;
static size_t mf_calls;
static size_t mf_cms_calls;
static size_t mf_poison;
static size_t mf_allocated;
static size_t mf_freed;
static size_t mf_reads;
static size_t mf_matched;
static size_t mf_seen;
static size_t mf_rgba;
static uint32_t mf_bad;
static float *mf_scratch;
static float mf_normal[72];
static float mf_frame[72];
static int mf_segment;
static int mf_changed;
static int mf_remainder;
static int mf_frame_ok;

static void *
mf_malloc(size_t n)
{
    void *p;
    size_t i;

    p = malloc(n);
    if (n == mf_n * 3u * sizeof(float) && p != NULL) {
        ++mf_allocated;
        mf_scratch = p;
        for (i = 0u; i < mf_n * 3u; ++i) {
            mf_scratch[i] = -12345.0f;
        }
    }
    return p;
}

static void
mf_free(void *p)
{
    size_t i;

    if (p != NULL && p == mf_scratch) {
        ++mf_freed;
        if (mf_phase == 0) {
            memcpy(mf_normal, p, mf_n * 3u * sizeof(float));
        }
        mf_segment = memcmp(p, mf_normal, 6u * sizeof(float)) == 0;
        mf_changed = mf_scratch[0] != -12345.0f;
        mf_remainder = 1;
        for (i = 6u; i < mf_n * 3u; ++i) {
            if (mf_scratch[i] != -12345.0f) { mf_remainder = 0; }
        }
        mf_scratch = NULL;
    }
    free(p);
}

static int
mf_curve(sixel_icc_curve_t const *curve, double x, int inverse, double *y)
{
    size_t channels;
    int ok;

    channels = tm_cases[mf_fixture].photo == 2 ? 3u : 1u;
    if (mf_mode == 1 || mf_mode == 2) {
        if (mf_curve_calls >= (mf_mode == 2 ? 0u : 2u * channels)) {
            ++mf_reject;
            return 0;
        }
    }
    ok = sixel_icc_curve_device(curve, x, inverse, y);
    if (ok) { ++mf_curve_calls; mf_success = mf_curve_calls / channels; }
    return ok;
}

#if defined(__APPLE__) && HAVE_COREGRAPHICS
static bool
mf_cs(ColorSyncTransformRef tr, size_t w, size_t h,
    void *dst, ColorSyncDataDepth dd, ColorSyncDataLayout dl, size_t ds,
    void const *src, ColorSyncDataDepth sd, ColorSyncDataLayout sl,
    size_t ss, CFDictionaryRef options)
{
    bool ok;

    if (mf_mode == 2) { ++mf_reject; return false; }
    if (mf_mode == 1) {
        /* A real native prefix precedes the modeled failure return. */
        ok = ColorSyncTransformConvert(tr, 2u, 1u, dst, dd, dl, ds,
                                        src, sd, sl, ss, options);
        mf_success += ok ? 2u : 0u;
        ++mf_reject;
        return false;
    }
    ok = ColorSyncTransformConvert(tr, w, h, dst, dd, dl, ds,
                                    src, sd, sl, ss, options);
    mf_success += ok ? w * h : 0u;
    if (ok && mf_mode == 3) {
        ((float *)dst)[w * h * 3u - 1u] = NAN;
        ++mf_poison;
    }
    return ok;
}
#endif

#if HAVE_LCMS2
static void
mf_lcms(cmsHTRANSFORM tr, void const *src, void *dst, cmsUInt32Number n)
{
    /* The native API is void: test its finite-result postcondition, not a
     * fabricated failure return. All native work happens before poisoning. */
    cmsDoTransform(tr, src, dst, n);
    mf_success += n;
    if (mf_mode == 3) {
        ((float *)dst)[n * 3u - 1u] = NAN;
        ++mf_poison;
    }
}
#endif

static int
mf_read(TIFF *tif, void *data, uint32_t row, uint16_t sample)
{
    uint32_t width;
    uint16_t channels;
    uint32_t word;
    uint32_t expected;
    size_t sample_bytes;
    size_t first;
    size_t i;
    int ok;

    ok = TIFFReadScanline(tif, data, row, sample);
    if (ok != 1) { return ok; }
    ++mf_reads;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
    first = (size_t)row * width * channels;
    sample_bytes = (size_t)tm_cases[mf_fixture].bits / 8u;
    for (i = 0u; i < (size_t)width * channels; ++i) {
        /* libtiff returns host-endian words, including UINT16 fixtures. */
        word = sample_bytes == 2u ? ((uint16_t const *)data)[i] : 0u;
        if (sample_bytes == sizeof(word)) {
            memcpy(&word, (unsigned char *)data + i * sample_bytes,
                   sizeof(word));
        }
        expected = first + i == 23u && mf_bad ? mf_bad :
            first + i < 24u ? tm_cases[mf_fixture].stored[first + i] : 0u;
        mf_matched += word == expected;
        mf_seen += first + i == 23u && mf_bad && word == mf_bad;
    }
    return ok;
}

static int
mf_rgba_read(TIFF *tif, uint32_t width, uint32_t height, uint32_t *raster,
             int orientation, int stop)
{
    ++mf_rgba;
    return TIFFReadRGBAImageOriented(tif, width, height, raster,
                                     orientation, stop);
}
#define SIXEL_CMS_PROFILE_BACKEND_BUILTIN mf_local_0
#define SIXEL_CMS_PROFILE_BACKEND_COLORSYNC mf_local_1
#define SIXEL_CMS_PROFILE_BACKEND_LCMS2 mf_local_2
#define SIXEL_CMS_PROFILE_BACKEND_NONE mf_local_3
#define g_sixel_cms_engine mf_local_4
#define g_sixel_cms_engine_mutex mf_local_5
#define g_sixel_cms_engine_mutex_once mf_local_6
#define g_sixel_cms_engine_mutex_ready mf_local_7
#define g_sixel_cms_engine_once mf_local_8
#define g_sixel_loader_libtiff_vtbl mf_local_9
#define load_tiff mf_local_10
#define load_with_libtiff mf_local_11
#define loader_can_try_libtiff mf_local_12
#define sixel_cdef_clone mf_local_13
#define sixel_cdef_finite mf_local_14
#define sixel_cdef_free mf_local_15
#define sixel_cdef_from_xyz mf_local_16
#define sixel_cdef_icc mf_local_17
#define sixel_cdef_matrix mf_local_18
#define sixel_cdef_name mf_local_19
#define sixel_cdef_output_ready mf_local_20
#define sixel_cdef_parse mf_local_21
#define sixel_cdef_pcs_scale mf_local_22
#define sixel_cdef_rgb mf_local_23
#define sixel_cdef_to_xyz mf_local_24
#define sixel_cms_allocate_profile mf_local_25
#define sixel_cms_apply_backend mf_local_26
#define sixel_cms_apply_matrix3 mf_local_27
#define sixel_cms_ascii_case_equal mf_local_28
#define sixel_cms_build_builtin_a2b_order mf_local_29
#define sixel_cms_build_intent_order mf_local_30
#define sixel_cms_build_rgb_to_xyz_matrix mf_local_31
#define sixel_cms_build_rgb_to_xyz_matrix_internal mf_local_32
#define sixel_cms_builtin_apply_cmyk_f32_intent mf_local_33
#define sixel_cms_builtin_apply_cmyk_u16_intent mf_local_34
#define sixel_cms_builtin_apply_cmyk_u8_intent mf_local_35
#define sixel_cms_builtin_apply_gray_u8_intent mf_local_36
#define sixel_cms_builtin_apply_rgb_f32_intent mf_local_37
#define sixel_cms_builtin_apply_rgb_u8_intent mf_local_38
#define sixel_cms_builtin_apply_src_to_xyz_intent mf_local_39
#define sixel_cms_builtin_apply_xyz_to_dst_intent mf_local_40
#define sixel_cms_builtin_convert_rgb8_to_rgbf32 mf_local_41
#define sixel_cms_builtin_kind_channel_count mf_local_42
#define sixel_cms_builtin_load_src_unit mf_local_43
#define sixel_cms_builtin_profile_can_convert mf_local_44
#define sixel_cms_builtin_profile_is_srgb mf_local_45
#define sixel_cms_builtin_store_dst_unit mf_local_46
#define sixel_cms_clamp_lab_ab mf_local_47
#define sixel_cms_clamp_unit mf_local_48
#define sixel_cms_close_profile mf_local_49
#define sixel_cms_colorsync_create_transform_one mf_local_50
#define sixel_cms_colorsync_intent_to_cfstring mf_local_51
#define sixel_cms_colorsync_lab_units mf_local_52
#define sixel_cms_colorsync_map_format mf_local_53
#define sixel_cms_colorsync_profile_channels mf_local_54
#define sixel_cms_colorsync_promote_source mf_local_55
#define sixel_cms_colorsync_source_matches mf_local_56
#define sixel_cms_convert_colorsync_linear mf_local_57
#define sixel_cms_convert_profile_to_cielab mf_local_58
#define sixel_cms_convert_profile_to_linearrgb mf_local_59
#define sixel_cms_convert_profile_to_srgb mf_local_60
#define sixel_cms_convert_rgbf32_gamma_to_linear mf_local_61
#define sixel_cms_convert_to_cielab_with_profile_bytes mf_local_62
#define sixel_cms_convert_to_linearrgb_with_profile_bytes mf_local_63
#define sixel_cms_convert_to_srgb_with_profile_bytes mf_local_64
#define sixel_cms_create_cielab_d50_profile mf_local_65
#define sixel_cms_create_linear_srgb_profile mf_local_66
#define sixel_cms_create_rgb_profile_from_gamma_chrm mf_local_67
#define sixel_cms_create_rgb_profile_from_gammas_chrm mf_local_68
#define sixel_cms_create_srgb_profile mf_local_69
#define sixel_cms_create_transform mf_local_70
#define sixel_cms_create_transform_builtin mf_local_71
#define sixel_cms_create_transform_colorsync mf_local_72
#define sixel_cms_create_transform_lcms2 mf_local_73
#define sixel_cms_decode_srgb_unit mf_local_74
#define sixel_cms_delete_transform mf_local_75
#define sixel_cms_do_transform mf_local_76
#define sixel_cms_do_transform_builtin mf_local_77
#define sixel_cms_do_transform_builtin_device_to_device mf_local_78
#define sixel_cms_do_transform_builtin_to_srgb mf_local_79
#define sixel_cms_encode_srgb_unit mf_local_80
#define sixel_cms_engine_from_string mf_local_81
#define sixel_cms_engine_lock mf_local_82
#define sixel_cms_engine_lock_init_once mf_local_83
#define sixel_cms_engine_to_string mf_local_84
#define sixel_cms_engine_unlock mf_local_85
#define sixel_cms_float_rgb mf_local_86
#define sixel_cms_float_source_xyz mf_local_87
#define sixel_cms_get_color_space mf_local_88
#define sixel_cms_get_engine mf_local_89
#define sixel_cms_intent_contains mf_local_90
#define sixel_cms_intent_name mf_local_91
#define sixel_cms_intent_to_a2b_slot mf_local_92
#define sixel_cms_lcms_input_units mf_local_93
#define sixel_cms_map_format_lcms mf_local_94
#define sixel_cms_matrix3_invert mf_local_95
#define sixel_cms_matrix3_invert_internal mf_local_96
#define sixel_cms_native_gamma mf_local_97
#define sixel_cms_open_profile_from_mem mf_local_98
#define sixel_cms_parse_icc_colorspace_signature mf_local_99
#define sixel_cms_prepare_linear_gray mf_local_100
#define sixel_cms_linear_unit_pixel tmf_mixed_unit
#define sixel_cms_apply_linear_colorsync tmf_mixed_apply
#define sixel_cms_prepare_linear_rgb mf_local_101
#define sixel_cms_profile mf_local_102
#define sixel_cms_profile_backend mf_local_103
#define sixel_cms_profile_backend_t mf_local_104
#define sixel_cms_profile_create_cielab_d50_builtin mf_local_105
#define sixel_cms_profile_create_cielab_d50_colorsync mf_local_106
#define sixel_cms_profile_create_cielab_d50_lcms2 mf_local_107
#define sixel_cms_profile_create_rgb_gamma_chrm_builtin mf_local_108
#define sixel_cms_profile_create_rgb_gamma_chrm_colorsync mf_local_109
#define sixel_cms_profile_create_rgb_gamma_chrm_lcms2 mf_local_110
#define sixel_cms_profile_create_srgb_builtin mf_local_111
#define sixel_cms_profile_create_srgb_colorsync mf_local_112
#define sixel_cms_profile_create_srgb_lcms2 mf_local_113
#define sixel_cms_profile_open_builtin mf_local_114
#define sixel_cms_profile_open_colorsync mf_local_115
#define sixel_cms_profile_open_lcms2 mf_local_116
#define sixel_cms_profile_t mf_local_117
#define sixel_cms_resolve_engine mf_local_118
#define sixel_cms_rgb_power_tag mf_local_119
#define sixel_cms_rgb_set_tag mf_local_120
#define sixel_cms_set_engine mf_local_121
#define sixel_cms_srgb_to_xyz_d65 mf_local_122
#define sixel_cms_srgb_unit_to_xyz_d50 mf_local_123
#define sixel_cms_trace_gamma mf_local_124
#define sixel_cms_trace_intent_order mf_local_125
#define sixel_cms_transform mf_local_126
#define sixel_cms_transform_engine mf_local_127
#define sixel_cms_transform_t mf_local_128
#define sixel_cms_xyz_d50_to_d65 mf_local_129
#define sixel_cms_xyz_d50_to_srgb_unit mf_local_130
#define sixel_cms_xyz_d65_to_d50 mf_local_131
#define sixel_cms_xyz_to_srgb_d65 mf_local_132
#define sixel_color_def mf_local_133
#define sixel_color_def_t mf_local_134
#define sixel_color_xform mf_local_135
#define sixel_color_xform_t mf_local_136
#define sixel_cxform_apply mf_local_137
#define sixel_cxform_free mf_local_138
#define sixel_cxform_new mf_local_139
#define sixel_loader_libtiff_component mf_local_140
#define sixel_loader_libtiff_component_t mf_local_141
#define sixel_loader_libtiff_load mf_local_142
#define sixel_loader_libtiff_name mf_local_143
#define sixel_loader_libtiff_new mf_local_144
#define sixel_loader_libtiff_placeholder mf_local_145
#define sixel_loader_libtiff_placeholder_function mf_local_146
#define sixel_loader_libtiff_predicate mf_local_147
#define sixel_loader_libtiff_ref mf_local_148
#define sixel_loader_libtiff_setopt mf_local_149
#define sixel_loader_libtiff_unref mf_local_150
#define tiff_clamp_lab_ab mf_local_151
#define tiff_clamp_unit mf_local_152
#define tiff_convert_embedded_icc_to_srgb mf_local_153
#define tiff_convert_embedded_icc_to_srgb_nolcms mf_local_155
#define tiff_decode_lab_u16_ab mf_local_157
#define tiff_decode_lab_u16_l mf_local_158
#define tiff_map_scanline_coordinates mf_local_159
#define tiff_memory_chunk mf_local_160
#define tiff_memory_chunk_t mf_local_161
#define tiff_memory_close mf_local_162
#define tiff_memory_map mf_local_163
#define tiff_memory_read mf_local_164
#define tiff_memory_seek mf_local_165
#define tiff_memory_size mf_local_166
#define tiff_memory_unmap mf_local_167
#define tiff_memory_write mf_local_168
#define tiff_normalize_lab_ab_sample mf_local_169
#define tiff_normalize_lab_l_sample mf_local_170
#define tiff_orientation_is_supported mf_local_171
#define tiff_photometric_allows_embedded_icc mf_local_172
#define tiff_try_load_high_precision mf_local_173
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


#define malloc mf_malloc
#define free mf_free
#define sixel_icc_curve_device mf_curve
#define ColorSyncTransformConvert mf_cs
#define cmsDoTransform mf_lcms
#include "src/cms.c"
#undef malloc
#undef free
#undef sixel_icc_curve_device
#undef ColorSyncTransformConvert
#undef cmsDoTransform
static int
mf_linear(unsigned char *pixels, int width, int height, int format,
    unsigned char const *profile, size_t size)
{
    ++mf_cms_calls;
    return mf_local_63(pixels, width, height, format, profile, size);
}
/* Integer sources now validate a borrowed profile before entering the same
 * transaction. Count that actual entry without weakening the existing
 * prefix, nonfinite, complete-source and recovery observations. */
static int
mf_profile_linear(unsigned char *pixels, int width, int height, int format,
                  sixel_cms_profile_t *profile)
{
    ++mf_cms_calls;
    return mf_local_59(pixels, width, height, format, profile);
}
#undef sixel_cms_convert_profile_to_linearrgb
#define sixel_cms_convert_profile_to_linearrgb mf_profile_linear
#undef sixel_cms_convert_to_linearrgb_with_profile_bytes
#define sixel_cms_convert_to_linearrgb_with_profile_bytes mf_linear
/* A loader-owned ICC retry must fail compilation, even in fault tests. */
#define sixel_icc_apply_rgb_float32(...) SIXEL_LOADER_MUST_USE_CMS_FACADE
#define TIFFReadScanline mf_read
#define TIFFReadRGBAImageOriented mf_rgba_read
#define tiff_logluv_xyz_to_linear mf_logluv_xyz
#define tiff_convert_samples_to_linear mf_byte
#define tiff_try_load_integer_alpha mf_alpha
#define tiff_read_separate_uint16 b1_namespace_5
#define tiff_publish_integer_alpha til_ns_8_0
#define tiff_integer_layout_t til_ns_8_1
#define tiff_scatter_integer_row til_ns_8_2
#define tiff_try_load_integer_layouts til_ns_8_3
#define tiff_lab8_ranges tlx_copy_7_ranges
#define tiff_lab8_to_linear tlx_copy_7_linear
#define tiff_convert_cmyk_to_linear tiff_managedf_ink_linear
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
#undef TIFFReadScanline
#undef TIFFReadRGBAImageOriented
#undef sixel_icc_apply_rgb_float32

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
#undef sixel_cms_convert_to_linearrgb_with_profile_bytes
static SIXELSTATUS
mf_capture(sixel_frame_t *frame, void *context)
{
    (void)context;
    ++mf_calls;
    mf_frame_ok = tiff_managed_check(frame, mf_fixture, mf_engine,
                                     mf_phase == 1);
    if (mf_frame_ok) {
        memcpy(mf_frame, frame->pixels.u8ptr, mf_n * 3u * sizeof(float));
    }
    return mf_frame_ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static SIXELSTATUS
mf_load(sixel_loader_component_t *component, sixel_chunk_t *chunk,
          int phase, int mode)
{
    SIXELSTATUS status;
    int cms;

    mf_phase = phase;
    mf_mode = mode;
    mf_success = mf_curve_calls = mf_reject = mf_calls = mf_cms_calls = 0u;
    mf_poison = mf_allocated = mf_freed = 0u;
    mf_reads = mf_matched = mf_seen = mf_rgba = 0u;
    mf_segment = mf_changed = mf_remainder = mf_frame_ok = 0;
    mf_scratch = NULL;
    cms = 1;
    fprintf(stderr, "TMFAULT|phase-begin=%d\n", phase);
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &cms);
    if (SIXEL_FAILED(status)) { return status; }
    status = component->vtbl->load(component, chunk, mf_capture, NULL);
    fprintf(stderr, "TMFAULT|phase=%d|mode=%d|status=%x|calls=%zu|"
        "success=%zu|reject=%zu|segment=%d|changed=%d|remainder=%d|"
        "alloc=%zu|freed=%zu|cms=%zu|poison=%zu|"
        "read=%zu|matched=%zu|seen=%zu|rgba=%zu|frame=%d\n",
        phase, mode, status, mf_calls, mf_success, mf_reject, mf_segment,
        mf_changed, mf_remainder, mf_allocated, mf_freed,
        mf_cms_calls, mf_poison, mf_reads, mf_matched, mf_seen, mf_rgba,
        mf_frame_ok);
    return status;
}
#endif

int
tiff_managed_failure(int argc, char **argv, int fixture, int engine,
                       int mode)
{
#if HAVE_LIBTIFF
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_chunk_t *bad_chunk;
    sixel_loader_component_t *component;
    sixel_cms_engine_t previous;
    float saved[72];
    void *object;
    SIXELSTATUS status;
    int ok;
    int qualified;
    int normal;
    int recovered;

    if (argc != (mode == 4 ? 3 : 2)) { return 2; }
    previous = mf_local_89();
    mf_local_121((sixel_cms_engine_t)engine);
    if ((int)mf_local_89() != engine) {
        mf_local_121(previous);
        return 77;
    }
    mf_fixture = fixture;
    mf_engine = engine;
    mf_n = (size_t)tm_cases[fixture].width * tm_cases[fixture].height;
    mf_bad = 0u;
    allocator = NULL;
    chunk = NULL;
    bad_chunk = NULL;
    component = NULL;
    object = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                        NULL))) { goto cleanup; }
    if (SIXEL_FAILED(sixel_chunk_create_from_source(&chunk, argv[1], 0,
                                  NULL, allocator))) { goto cleanup; }
    if (SIXEL_FAILED(mf_local_144(allocator, &object))) {
        goto cleanup;
    }
    component = object;
    status = mf_load(component, chunk, 0, 0);
    normal = status == SIXEL_OK && mf_calls == 1u && mf_frame_ok &&
        mf_allocated == 1u && mf_freed == 1u && mf_scratch == NULL;
    memcpy(saved, mf_frame, mf_n * 3u * sizeof(float));
    if (mode == 4) {
        if (SIXEL_FAILED(sixel_chunk_create_from_source(&bad_chunk, argv[2],
                            0, NULL, allocator))) { goto cleanup; }
        mf_bad = fixture == 0 ? 0x7fc12345u :
                 fixture == 1 ? 0x7f800000u : 0xff800000u;
    }
    status = mf_load(component, mode == 4 ? bad_chunk : chunk, 1, mode);
    qualified = mf_success == 2u && mf_reject > 0u &&
        mf_segment && mf_changed && mf_remainder;
    if (mode == 4) {
        ok = status == SIXEL_TIFF_ERROR && mf_calls == 0u &&
             mf_matched == 24u && mf_seen == 1u &&
             mf_reads == (size_t)tm_cases[fixture].height &&
             mf_cms_calls == 0u && mf_allocated == 0u;
    } else {
        ok = status == SIXEL_OK && mf_calls == 1u && mf_frame_ok &&
             mf_allocated == 1u && mf_freed == 1u && mf_scratch == NULL &&
             mf_cms_calls == 1u;
        if (mode == 1) { ok = ok && qualified; }
        if (mode == 2) {
            ok = ok && !qualified && mf_success == 0u && mf_reject > 0u &&
                 !mf_changed && mf_remainder;
        }
        if (mode == 3) {
            ok = ok && mf_success == mf_n && mf_poison == 1u &&
                 mf_segment && mf_changed;
        }
    }
    ok = ok && mf_rgba == 0u;
    fprintf(stderr, "TMFAULT|qualified=%d|accepted=%d\n", qualified, ok);
    mf_bad = 0u;
    status = mf_load(component, chunk, 2, 0);
    recovered = status == SIXEL_OK && mf_calls == 1u && mf_frame_ok &&
        memcmp(mf_frame, saved, mf_n * 3u * sizeof(float)) == 0 &&
        mf_allocated == 1u && mf_freed == 1u && mf_scratch == NULL;
    fprintf(stderr, "TMFAULT|normal=%d|recovered=%d\n", normal, recovered);
    ok = ok && normal && recovered;
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    if (bad_chunk != NULL) { bad_chunk->vtbl->unref(bad_chunk); }
    sixel_allocator_unref(allocator);
    mf_local_121(previous);
    return ok ? 0 : 1;
#else
    (void)argc; (void)argv; (void)fixture; (void)engine; (void)mode;
    return 77;
#endif
}
