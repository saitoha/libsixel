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
#include "src/chunk-view.h"
#include "tests/data/color-numeric/tiff-byte.h"
#include "tiff_byte_test_common.h"
#if HAVE_LIBTIFF
#include <tiffio.h>
#if defined(__APPLE__) && HAVE_COREGRAPHICS
#include <ColorSync/ColorSync.h>
#endif
#if HAVE_LCMS2
#include <lcms2.h>
#endif
static int bf_mode;
static int bf_phase;
static int bf_fixture;
static int bf_engine;
static size_t bf_n;
static size_t bf_success;
static size_t bf_curve_calls;
static size_t bf_reject;
static size_t bf_calls;
static size_t bf_cms_calls;
static size_t bf_legacy_calls;
static size_t bf_poison;
static size_t bf_allocated;
static size_t bf_freed;
static size_t bf_reads;
static size_t bf_matched;
static size_t bf_seen;
static size_t bf_rgba;
static int bf_rollback;
static int bf_injected;
static int bf_armed;
static size_t bf_live;
static float *bf_scratch;
static float bf_normal[72];
static float bf_frame[72];
static int bf_segment;
static int bf_changed;
static int bf_remainder;
static int bf_frame_ok;

static void *
bf_malloc(size_t n)
{
    void *p;
    size_t i;

    p = malloc(n);
    if (n == bf_n * 3u * sizeof(float) && p != NULL) {
        ++bf_allocated;
        bf_scratch = p;
        for (i = 0u; i < bf_n * 3u; ++i) {
            bf_scratch[i] = 0.125f;
        }
    }
    return p;
}

static void
bf_free(void *p)
{
    size_t i;

    if (p != NULL && p == bf_scratch) {
        ++bf_freed;
        if (bf_phase == 0) {
            memcpy(bf_normal, p, bf_n * 3u * sizeof(float));
        }
        bf_segment = memcmp(p, bf_normal, 6u * sizeof(float)) == 0;
        bf_changed = bf_scratch[0] != 0.125f;
        bf_remainder = 1;
        for (i = 6u; i < bf_n * 3u; ++i) {
            if (bf_scratch[i] != 0.125f) { bf_remainder = 0; }
        }
        bf_scratch = NULL;
    }
    free(p);
}

static void *
bf_image_malloc(size_t n)
{
    void *p;

    if (bf_armed && bf_mode == 4 && n == 18u * sizeof(float)) {
        ++bf_injected;
        return NULL;
    }
    p = malloc(n);
    if (p != NULL) { ++bf_live; }
    return p;
}

static void *
bf_image_calloc(size_t n, size_t size)
{
    void *p;

    p = calloc(n, size);
    if (p != NULL) { ++bf_live; }
    return p;
}

static void *
bf_image_realloc(void *ptr, size_t size)
{
    void *p;
    int empty;

    empty = ptr == NULL;
    p = realloc(ptr, size);
    if (empty && p != NULL) { ++bf_live; }
    return p;
}

static void
bf_image_free(void *p)
{
    if (p != NULL) { --bf_live; }
    free(p);
}

static int
bf_curve(sixel_icc_curve_t const *curve, double x, int inverse, double *y)
{
    size_t channels;
    int ok;

    channels = tb_cases[bf_fixture].photo == 2 ? 3u : 1u;
    if (bf_mode == 1 || bf_mode == 2) {
        if (bf_curve_calls >= (bf_mode == 2 ? 0u : 2u * channels)) {
            ++bf_reject;
            return 0;
        }
    }
    ok = sixel_icc_curve_device(curve, x, inverse, y);
    if (ok) { ++bf_curve_calls; bf_success = bf_curve_calls / channels; }
    return ok;
}

#if defined(__APPLE__) && HAVE_COREGRAPHICS
static bool
bf_cs(ColorSyncTransformRef tr, size_t w, size_t h,
    void *dst, ColorSyncDataDepth dd, ColorSyncDataLayout dl, size_t ds,
    void const *src, ColorSyncDataDepth sd, ColorSyncDataLayout sl,
    size_t ss, CFDictionaryRef options)
{
    bool ok;

    if (bf_mode == 2) { ++bf_reject; return false; }
    if (bf_mode == 1) {
        /* A real native prefix precedes the modeled failure return. */
        ok = ColorSyncTransformConvert(tr, 2u, 1u, dst, dd, dl, ds,
                                        src, sd, sl, ss, options);
        bf_success += ok ? 2u : 0u;
        ++bf_reject;
        return false;
    }
    ok = ColorSyncTransformConvert(tr, w, h, dst, dd, dl, ds,
                                    src, sd, sl, ss, options);
    bf_success += ok ? w * h : 0u;
    if (ok && bf_mode == 3) {
        ((float *)dst)[w * h * 3u - 1u] = NAN;
        ++bf_poison;
    }
    return ok;
}
#endif

#if HAVE_LCMS2
static void
bf_lcms(cmsHTRANSFORM tr, void const *src, void *dst, cmsUInt32Number n)
{
    /* The native API is void: test its finite-result postcondition, not a
     * fabricated failure return. All native work happens before poisoning. */
    cmsDoTransform(tr, src, dst, n);
    bf_success += n;
    if (bf_mode == 3) {
        ((float *)dst)[n * 3u - 1u] = NAN;
        ++bf_poison;
    }
}
#endif

static int
bf_legacy(float *pixels, size_t n, sixel_icc_profile_t const *profile)
{
    ++bf_legacy_calls;
    return sixel_icc_apply_rgb_float32(pixels, n, profile);
}

static int
bf_read(TIFF *tif, void *data, uint32_t row, uint16_t sample)
{
    uint32_t width;
    uint16_t channels;
    unsigned char word;
    uint32_t expected;
    size_t first;
    size_t i;
    int ok;

    if (bf_mode == 5 && row == 1u && bf_reads == 1u) {
        ++bf_injected;
        return -1;
    }
    ok = TIFFReadScanline(tif, data, row, sample);
    if (ok != 1) { return ok; }
    ++bf_reads;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
    first = (size_t)row * width * channels;
    for (i = 0u; i < (size_t)width * channels; ++i) {
        word = ((unsigned char const *)data)[i];
        expected = tb_cases[bf_fixture].stored[first + i];
        bf_matched += word == expected;
    }
    return ok;
}

static int
bf_rgba_read(TIFF *tif, uint32_t width, uint32_t height, uint32_t *raster,
             int orientation, int stop)
{
    ++bf_rgba;
    return TIFFReadRGBAImageOriented(tif, width, height, raster,
                                     orientation, stop);
}
#define SIXEL_CMS_PROFILE_BACKEND_BUILTIN bf_local_0
#define SIXEL_CMS_PROFILE_BACKEND_COLORSYNC bf_local_1
#define SIXEL_CMS_PROFILE_BACKEND_LCMS2 bf_local_2
#define SIXEL_CMS_PROFILE_BACKEND_NONE bf_local_3
#define g_sixel_cms_engine bf_local_4
#define g_sixel_cms_engine_mutex bf_local_5
#define g_sixel_cms_engine_mutex_once bf_local_6
#define g_sixel_cms_engine_mutex_ready bf_local_7
#define g_sixel_cms_engine_once bf_local_8
#define g_sixel_loader_libtiff_vtbl bf_local_9
#define load_tiff bf_local_10
#define load_with_libtiff bf_local_11
#define loader_can_try_libtiff bf_local_12
#define sixel_cdef_clone bf_local_13
#define sixel_cdef_finite bf_local_14
#define sixel_cdef_free bf_local_15
#define sixel_cdef_from_xyz bf_local_16
#define sixel_cdef_icc bf_local_17
#define sixel_cdef_matrix bf_local_18
#define sixel_cdef_name bf_local_19
#define sixel_cdef_output_ready bf_local_20
#define sixel_cdef_parse bf_local_21
#define sixel_cdef_pcs_scale bf_local_22
#define sixel_cdef_rgb bf_local_23
#define sixel_cdef_to_xyz bf_local_24
#define sixel_cms_allocate_profile bf_local_25
#define sixel_cms_apply_backend bf_local_26
#define sixel_cms_apply_matrix3 bf_local_27
#define sixel_cms_ascii_case_equal bf_local_28
#define sixel_cms_build_builtin_a2b_order bf_local_29
#define sixel_cms_build_intent_order bf_local_30
#define sixel_cms_build_rgb_to_xyz_matrix bf_local_31
#define sixel_cms_build_rgb_to_xyz_matrix_internal bf_local_32
#define sixel_cms_builtin_apply_cmyk_f32_intent bf_local_33
#define sixel_cms_builtin_apply_cmyk_u16_intent bf_local_34
#define sixel_cms_builtin_apply_cmyk_u8_intent bf_local_35
#define sixel_cms_builtin_apply_gray_u8_intent bf_local_36
#define sixel_cms_builtin_apply_rgb_f32_intent bf_local_37
#define sixel_cms_builtin_apply_rgb_u8_intent bf_local_38
#define sixel_cms_builtin_apply_src_to_xyz_intent bf_local_39
#define sixel_cms_builtin_apply_xyz_to_dst_intent bf_local_40
#define sixel_cms_builtin_convert_rgb8_to_rgbf32 bf_local_41
#define sixel_cms_builtin_kind_channel_count bf_local_42
#define sixel_cms_builtin_load_src_unit bf_local_43
#define sixel_cms_builtin_profile_can_convert bf_local_44
#define sixel_cms_builtin_profile_is_srgb bf_local_45
#define sixel_cms_builtin_store_dst_unit bf_local_46
#define sixel_cms_clamp_lab_ab bf_local_47
#define sixel_cms_clamp_unit bf_local_48
#define sixel_cms_close_profile bf_local_49
#define sixel_cms_colorsync_create_transform_one bf_local_50
#define sixel_cms_colorsync_intent_to_cfstring bf_local_51
#define sixel_cms_colorsync_lab_units bf_local_52
#define sixel_cms_colorsync_map_format bf_local_53
#define sixel_cms_colorsync_profile_channels bf_local_54
#define sixel_cms_colorsync_promote_source bf_local_55
#define sixel_cms_colorsync_source_matches bf_local_56
#define sixel_cms_convert_colorsync_linear bf_local_57
#define sixel_cms_convert_profile_to_cielab bf_local_58
#define sixel_cms_convert_profile_to_linearrgb bf_local_59
#define sixel_cms_convert_profile_to_srgb bf_local_60
#define sixel_cms_convert_rgbf32_gamma_to_linear bf_local_61
#define sixel_cms_convert_to_cielab_with_profile_bytes bf_local_62
#define sixel_cms_convert_to_linearrgb_with_profile_bytes bf_local_63
#define sixel_cms_convert_to_srgb_with_profile_bytes bf_local_64
#define sixel_cms_create_cielab_d50_profile bf_local_65
#define sixel_cms_create_linear_srgb_profile bf_local_66
#define sixel_cms_create_rgb_profile_from_gamma_chrm bf_local_67
#define sixel_cms_create_rgb_profile_from_gammas_chrm bf_local_68
#define sixel_cms_create_srgb_profile bf_local_69
#define sixel_cms_create_transform bf_local_70
#define sixel_cms_create_transform_builtin bf_local_71
#define sixel_cms_create_transform_colorsync bf_local_72
#define sixel_cms_create_transform_lcms2 bf_local_73
#define sixel_cms_decode_srgb_unit bf_local_74
#define sixel_cms_delete_transform bf_local_75
#define sixel_cms_do_transform bf_local_76
#define sixel_cms_do_transform_builtin bf_local_77
#define sixel_cms_do_transform_builtin_device_to_device bf_local_78
#define sixel_cms_do_transform_builtin_to_srgb bf_local_79
#define sixel_cms_encode_srgb_unit bf_local_80
#define sixel_cms_engine_from_string bf_local_81
#define sixel_cms_engine_lock bf_local_82
#define sixel_cms_engine_lock_init_once bf_local_83
#define sixel_cms_engine_to_string bf_local_84
#define sixel_cms_engine_unlock bf_local_85
#define sixel_cms_float_rgb bf_local_86
#define sixel_cms_float_source_xyz bf_local_87
#define sixel_cms_get_color_space bf_local_88
#define sixel_cms_get_engine bf_local_89
#define sixel_cms_intent_contains bf_local_90
#define sixel_cms_intent_name bf_local_91
#define sixel_cms_intent_to_a2b_slot bf_local_92
#define sixel_cms_lcms_input_units bf_local_93
#define sixel_cms_map_format_lcms bf_local_94
#define sixel_cms_matrix3_invert bf_local_95
#define sixel_cms_matrix3_invert_internal bf_local_96
#define sixel_cms_native_gamma bf_local_97
#define sixel_cms_open_profile_from_mem bf_local_98
#define sixel_cms_parse_icc_colorspace_signature bf_local_99
#define sixel_cms_prepare_linear_gray bf_local_100
#define sixel_cms_linear_unit_pixel tbf_mixed_unit
#define sixel_cms_apply_linear_colorsync tbf_mixed_apply
#define sixel_cms_prepare_linear_rgb bf_local_101
#define sixel_cms_profile bf_local_102
#define sixel_cms_profile_backend bf_local_103
#define sixel_cms_profile_backend_t bf_local_104
#define sixel_cms_profile_create_cielab_d50_builtin bf_local_105
#define sixel_cms_profile_create_cielab_d50_colorsync bf_local_106
#define sixel_cms_profile_create_cielab_d50_lcms2 bf_local_107
#define sixel_cms_profile_create_rgb_gamma_chrm_builtin bf_local_108
#define sixel_cms_profile_create_rgb_gamma_chrm_colorsync bf_local_109
#define sixel_cms_profile_create_rgb_gamma_chrm_lcms2 bf_local_110
#define sixel_cms_profile_create_srgb_builtin bf_local_111
#define sixel_cms_profile_create_srgb_colorsync bf_local_112
#define sixel_cms_profile_create_srgb_lcms2 bf_local_113
#define sixel_cms_profile_open_builtin bf_local_114
#define sixel_cms_profile_open_colorsync bf_local_115
#define sixel_cms_profile_open_lcms2 bf_local_116
#define sixel_cms_profile_t bf_local_117
#define sixel_cms_resolve_engine bf_local_118
#define sixel_cms_rgb_power_tag bf_local_119
#define sixel_cms_rgb_set_tag bf_local_120
#define sixel_cms_set_engine bf_local_121
#define sixel_cms_srgb_to_xyz_d65 bf_local_122
#define sixel_cms_srgb_unit_to_xyz_d50 bf_local_123
#define sixel_cms_trace_gamma bf_local_124
#define sixel_cms_trace_intent_order bf_local_125
#define sixel_cms_transform bf_local_126
#define sixel_cms_transform_engine bf_local_127
#define sixel_cms_transform_t bf_local_128
#define sixel_cms_xyz_d50_to_d65 bf_local_129
#define sixel_cms_xyz_d50_to_srgb_unit bf_local_130
#define sixel_cms_xyz_d65_to_d50 bf_local_131
#define sixel_cms_xyz_to_srgb_d65 bf_local_132
#define sixel_color_def bf_local_133
#define sixel_color_def_t bf_local_134
#define sixel_color_xform bf_local_135
#define sixel_color_xform_t bf_local_136
#define sixel_cxform_apply bf_local_137
#define sixel_cxform_free bf_local_138
#define sixel_cxform_new bf_local_139
#define sixel_loader_libtiff_component bf_local_140
#define sixel_loader_libtiff_component_t bf_local_141
#define sixel_loader_libtiff_load bf_local_142
#define sixel_loader_libtiff_name bf_local_143
#define sixel_loader_libtiff_new bf_local_144
#define sixel_loader_libtiff_placeholder bf_local_145
#define sixel_loader_libtiff_placeholder_function bf_local_146
#define sixel_loader_libtiff_predicate bf_local_147
#define sixel_loader_libtiff_ref bf_local_148
#define sixel_loader_libtiff_setopt bf_local_149
#define sixel_loader_libtiff_unref bf_local_150
#define tiff_clamp_lab_ab bf_local_151
#define tiff_clamp_unit bf_local_152
#define tiff_convert_embedded_icc_to_srgb bf_local_153
#define tiff_convert_embedded_icc_to_srgb_float32 bf_local_154
#define tiff_convert_embedded_icc_to_srgb_nolcms bf_local_155
#define tiff_convert_rgbf32_gamma_to_linear bf_local_156
#define tiff_decode_lab_u16_ab bf_local_157
#define tiff_decode_lab_u16_l bf_local_158
#define tiff_map_scanline_coordinates bf_local_159
#define tiff_memory_chunk bf_local_160
#define tiff_memory_chunk_t bf_local_161
#define tiff_memory_close bf_local_162
#define tiff_memory_map bf_local_163
#define tiff_memory_read bf_local_164
#define tiff_memory_seek bf_local_165
#define tiff_memory_size bf_local_166
#define tiff_memory_unmap bf_local_167
#define tiff_memory_write bf_local_168
#define tiff_normalize_lab_ab_sample bf_local_169
#define tiff_normalize_lab_l_sample bf_local_170
#define tiff_orientation_is_supported bf_local_171
#define tiff_photometric_allows_embedded_icc bf_local_172
#define tiff_try_load_high_precision bf_local_173
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


#define malloc bf_malloc
#define free bf_free
#define sixel_icc_curve_device bf_curve
#define ColorSyncTransformConvert bf_cs
#define cmsDoTransform bf_lcms
#include "src/cms.c"
#undef malloc
#undef free
#undef sixel_icc_curve_device
#undef ColorSyncTransformConvert
#undef cmsDoTransform
/* Observe rollback at the borrowed-profile boundary, before compaction. */
static int
bf_linear(unsigned char *pixels, int width, int height, int format,
          sixel_cms_profile_t *profile)
{
    unsigned char before[18u * sizeof(float)];
    int ok;

    ++bf_cms_calls;
    memcpy(before, pixels, sizeof(before));
    ok = bf_local_59(pixels, width, height, format, profile);
    bf_rollback = ok || memcmp(before, pixels, sizeof(before)) == 0;
    return ok;
}
#undef sixel_cms_convert_profile_to_linearrgb
#define sixel_cms_convert_profile_to_linearrgb bf_linear
#define sixel_icc_apply_rgb_float32 bf_legacy
#define TIFFReadScanline bf_read
#define TIFFReadRGBAImageOriented bf_rgba_read
#define tiff_logluv_xyz_to_linear bf_logluv_xyz
#define tiff_convert_samples_to_linear bf_byte
#define tiff_try_load_integer_alpha bf_alpha
#include "src/loader-libtiff.c"
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
#undef tiff_convert_embedded_icc_to_srgb_float32
#undef tiff_convert_embedded_icc_to_srgb_nolcms
#undef tiff_convert_rgbf32_gamma_to_linear
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
bf_capture(sixel_frame_t *frame, void *context)
{
    (void)context;
    ++bf_calls;
    bf_frame_ok = tiff_byte_check(frame, bf_fixture, bf_engine,
                                     bf_phase == 1);
    if (bf_frame_ok && bf_phase != 1) {
        memcpy(bf_frame, frame->pixels.u8ptr, bf_n * 3u * sizeof(float));
    }
    return bf_frame_ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static SIXELSTATUS
bf_load(sixel_loader_component_t *component, sixel_chunk_t *chunk,
          int phase, int mode)
{
    SIXELSTATUS status;
    int cms;

    bf_phase = phase;
    bf_mode = mode;
    bf_success = bf_curve_calls = bf_reject = bf_calls = bf_cms_calls = 0u;
    bf_legacy_calls = bf_poison = bf_allocated = bf_freed = 0u;
    bf_reads = bf_matched = bf_seen = bf_rgba = 0u;
    bf_segment = bf_changed = bf_remainder = bf_frame_ok = 0;
    bf_scratch = NULL;
    bf_rollback = bf_injected = 0;
    cms = 1;
    fprintf(stderr, "TBFAULT|phase-begin=%d\n", phase);
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &cms);
    if (SIXEL_FAILED(status)) { return status; }
    bf_armed = 1;
    status = component->vtbl->load(component, chunk, bf_capture, NULL);
    bf_armed = 0;
    fprintf(stderr, "TBFAULT|phase=%d|mode=%d|status=%x|calls=%zu|"
        "success=%zu|reject=%zu|segment=%d|changed=%d|remainder=%d|"
        "alloc=%zu|freed=%zu|legacy=%zu|cms=%zu|poison=%zu|"
        "read=%zu|matched=%zu|seen=%zu|rgba=%zu|frame=%d\n",
        phase, mode, status, bf_calls, bf_success, bf_reject, bf_segment,
        bf_changed, bf_remainder, bf_allocated, bf_freed, bf_legacy_calls,
        bf_cms_calls, bf_poison, bf_reads, bf_matched, bf_seen, bf_rgba,
        bf_frame_ok);
    return status;
}
#endif

#if HAVE_LIBTIFF
static int
bf_run(char const *path, int fixture, int engine, int mode)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    sixel_cms_engine_t previous;
    float saved[18];
    unsigned char original[4096];
    size_t source_size;
    void *object;
    SIXELSTATUS status;
    int ok;
    int qualified;
    int baseline;
    int recovered;

    previous = bf_local_89();
    bf_local_121((sixel_cms_engine_t)engine);
    if ((int)bf_local_89() != engine) {
        bf_local_121(previous);
        return 77;
    }
    bf_fixture = fixture;
    bf_engine = engine;
    bf_n = 6u;
    bf_live = 0u;
    bf_armed = 0;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    ok = 0;
    source_size = 0u;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, bf_image_malloc,
        bf_image_calloc, bf_image_realloc, bf_image_free))) { goto cleanup; }
    if (SIXEL_FAILED(sixel_chunk_create_from_source(&chunk, path, 0,
                                  NULL, allocator))) { goto cleanup; }
    source_size = sixel_chunk_get_size(chunk);
    if (source_size > sizeof(original)) { goto cleanup; }
    memcpy(original, sixel_chunk_get_buffer(chunk), source_size);
    if (SIXEL_FAILED(bf_local_144(allocator, &object))) { goto cleanup; }
    component = object;
    if (mode >= 4) {
        status = bf_load(component, chunk, 1, mode);
        ok = SIXEL_FAILED(status) && bf_calls == 0u && bf_rgba == 0u &&
             bf_injected == 1 && bf_cms_calls == 0u &&
             bf_reads == (mode == 5 ? 1u : 0u) &&
             bf_matched == (mode == 5 ? 9u : 0u);
        goto cleanup;
    }
    status = bf_load(component, chunk, 0, 0);
    baseline = status == SIXEL_OK && bf_calls == 1u && bf_frame_ok &&
        bf_allocated == 1u && bf_freed == 1u && bf_scratch == NULL &&
        bf_matched == (tb_cases[fixture].photo == 2 ? 18u : 6u) &&
        bf_legacy_calls == 0u && bf_cms_calls == 1u && bf_rgba == 0u;
    memcpy(saved, bf_frame, sizeof(saved));
    status = bf_load(component, chunk, 1, mode);
    qualified = bf_success == 2u && bf_reject > 0u &&
        bf_segment && bf_changed && bf_remainder;
    ok = status == SIXEL_OK && bf_calls == 1u && bf_frame_ok &&
         bf_allocated == 1u && bf_freed == 1u && bf_scratch == NULL &&
         bf_cms_calls == 1u && bf_rollback && bf_rgba == 0u &&
         bf_matched == (tb_cases[fixture].photo == 2 ? 18u : 6u) &&
         bf_legacy_calls == 0u;
    if (mode == 1) { ok = ok && qualified; }
    if (mode == 3) {
        ok = ok && bf_success == bf_n && bf_poison == 1u &&
             bf_segment && bf_changed;
    }
    fprintf(stderr, "TBFAULT|engine=%d|qualified=%d|rollback=%d|ok=%d\n",
            engine, qualified, bf_rollback, ok);
    status = bf_load(component, chunk, 2, 0);
    recovered = status == SIXEL_OK && bf_calls == 1u && bf_frame_ok &&
        memcmp(bf_frame, saved, sizeof(saved)) == 0 &&
        bf_allocated == 1u && bf_freed == 1u && bf_scratch == NULL;
    ok = ok && baseline && recovered;
cleanup:
    if (ok && chunk != NULL) {
        ok = source_size <= sizeof(original) &&
             memcmp(original, sixel_chunk_get_buffer(chunk), source_size) == 0;
    }
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    bf_local_121(previous);
    return ok && bf_live == 0u ? 0 : 1;
}
#endif

int
tiff_byte_faults(int argc, char **argv, int fixture)
{
#if HAVE_LIBTIFF
    char path[4096];
    int engine;
    int result;

    if (argc != 2) { return 2; }
    if (fixture == 0) { fixture = 3; }
    snprintf(path, sizeof(path), "%s/tiff-byte-%s-icc.tif", argv[1],
             tb_cases[fixture].name);
    for (engine = 2; engine <= 4; ++engine) {
        result = bf_run(path, fixture, engine, engine == 3 ? 3 : 1);
        if (result != 0 && result != 77) { return result; }
        if (engine == 2 && result == 77) { return 77; }
        if (engine == 4 && result == 0) {
            result = bf_run(path, fixture, engine, 3);
            if (result != 0) { return result; }
        }
    }
    return 0;
#else
    (void)argc; (void)argv; (void)fixture;
    return 77;
#endif
}

int
tiff_byte_decode(int argc, char **argv, int mode)
{
#if HAVE_LIBTIFF
    char path[4096];

    if (argc != 2) { return 2; }
    snprintf(path, sizeof(path), "%s/tiff-byte-rgb-o1-s1.tif", argv[1]);
    return bf_run(path, 0, 2, mode);
#else
    (void)argc; (void)argv; (void)mode;
    return 77;
#endif
}
