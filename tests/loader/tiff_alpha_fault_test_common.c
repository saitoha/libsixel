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
#include "tests/data/color-numeric/tiff-alpha.h"
#include "tiff_alpha_test_common.h"

#if HAVE_LIBTIFF
#include <tiffio.h>
#if defined(__APPLE__) && HAVE_COREGRAPHICS
#include <ColorSync/ColorSync.h>
#endif
#if HAVE_LCMS2
#include <lcms2.h>
#endif
static int af_background;
static int af_source_ok;
static int af_cms_result;
static int af_alpha_ok;
static int af_color_allocated;
static unsigned char *af_alpha;
static int af_mode;
static int af_phase;
static int af_fixture;
static int af_engine;
static size_t af_n;
static size_t af_success;
static size_t af_curve_calls;
static size_t af_reject;
static size_t af_calls;
static size_t af_cms_calls;
static size_t af_poison;
static size_t af_allocated;
static size_t af_freed;
static size_t af_reads;
static size_t af_matched;
static size_t af_seen;
static size_t af_rgba;
static int af_rollback;
static int af_injected;
static int af_armed;
static size_t af_live;
static float *af_scratch;
static float af_normal[72];
static float af_frame[72];
static int af_segment;
static int af_changed;
static int af_remainder;
static int af_frame_ok;

static void *
af_malloc(size_t n)
{
    void *p;
    size_t i;

    p = malloc(n);
    if (n == af_n * 3u * sizeof(float) && p != NULL) {
        ++af_allocated;
        af_scratch = p;
        for (i = 0u; i < af_n * 3u; ++i) {
            af_scratch[i] = 0.125f;
        }
    }
    return p;
}

static void
af_free(void *p)
{
    size_t i;

    if (p != NULL && p == af_scratch) {
        ++af_freed;
        if (af_phase == 0) {
            memcpy(af_normal, p, af_n * 3u * sizeof(float));
        }
        af_segment = memcmp(p, af_normal, 6u * sizeof(float)) == 0;
        af_changed = af_scratch[0] != 0.125f;
        af_remainder = 1;
        for (i = 6u; i < af_n * 3u; ++i) {
            if (af_scratch[i] != 0.125f) { af_remainder = 0; }
        }
        af_scratch = NULL;
    }
    free(p);
}

static void *
af_image_malloc(size_t n)
{
    void *p;

    if (af_armed) {
        fprintf(stderr, "AALLOC|phase=%d|mode=%d|bytes=%zu|color=%d\n",
                af_phase, af_mode, n, af_color_allocated);
    }
    if (af_armed && af_mode == 4 && n == 12u * sizeof(float)) {
        ++af_injected;
        return NULL;
    }
    if (af_armed && af_mode == 6 && af_color_allocated &&
        n == af_n * (size_t)(ta_cases[af_fixture].bits / 8)) {
        ++af_injected;
        return NULL;
    }
    if (af_armed && af_mode == 7 && !af_color_allocated &&
        n == (ta_cases[af_fixture].photo == 2 ? 8u : 4u) *
        (size_t)(ta_cases[af_fixture].bits / 8)) {
        ++af_injected;
        return NULL;
    }
    p = malloc(n);
    if (p != NULL) { ++af_live; }
    if (af_armed && n == 12u * sizeof(float) && p != NULL) {
        af_color_allocated = 1;
    }
    if (af_armed && af_color_allocated &&
        n == af_n * (size_t)(ta_cases[af_fixture].bits / 8)) {
        af_alpha = p;
    }
    return p;
}

static void *
af_image_calloc(size_t n, size_t size)
{
    void *p;

    p = calloc(n, size);
    if (p != NULL) { ++af_live; }
    return p;
}

static void *
af_image_realloc(void *ptr, size_t size)
{
    void *p;
    int empty;

    empty = ptr == NULL;
    p = realloc(ptr, size);
    if (empty && p != NULL) { ++af_live; }
    return p;
}

static void
af_image_free(void *p)
{
    if (p != NULL) { --af_live; }
    free(p);
}

static int
af_curve(sixel_icc_curve_t const *curve, double x, int inverse, double *y)
{
    size_t channels;
    int ok;

    channels = ta_cases[af_fixture].photo == 2 ? 3u : 1u;
    if (af_mode == 1 || af_mode == 2) {
        if (af_curve_calls >= (af_mode == 2 ? 0u : 2u * channels)) {
            ++af_reject;
            return 0;
        }
    }
    ok = sixel_icc_curve_device(curve, x, inverse, y);
    if (ok) { ++af_curve_calls; af_success = af_curve_calls / channels; }
    return ok;
}

#if defined(__APPLE__) && HAVE_COREGRAPHICS
static bool
af_cs(ColorSyncTransformRef tr, size_t w, size_t h,
    void *dst, ColorSyncDataDepth dd, ColorSyncDataLayout dl, size_t ds,
    void const *src, ColorSyncDataDepth sd, ColorSyncDataLayout sl,
    size_t ss, CFDictionaryRef options)
{
    bool ok;

    if (af_mode == 2) { ++af_reject; return false; }
    if (af_mode == 1) {
        /* A real native prefix precedes the modeled failure return. */
        ok = ColorSyncTransformConvert(tr, 2u, 1u, dst, dd, dl, ds,
                                        src, sd, sl, ss, options);
        af_success += ok ? 2u : 0u;
        ++af_reject;
        return false;
    }
    ok = ColorSyncTransformConvert(tr, w, h, dst, dd, dl, ds,
                                    src, sd, sl, ss, options);
    af_success += ok ? w * h : 0u;
    if (ok && af_mode == 3) {
        ((float *)dst)[w * h * 3u - 1u] = NAN;
        ++af_poison;
    }
    return ok;
}
#endif

#if HAVE_LCMS2
static void
af_lcms(cmsHTRANSFORM tr, void const *src, void *dst, cmsUInt32Number n)
{
    /* The native API is void: test its finite-result postcondition, not a
     * fabricated failure return. All native work happens before poisoning. */
    cmsDoTransform(tr, src, dst, n);
    af_success += n;
    if (af_mode == 3) {
        ((float *)dst)[n * 3u - 1u] = NAN;
        ++af_poison;
    }
}
#endif

static int
af_read(TIFF *tif, void *data, uint32_t row, uint16_t sample)
{
    uint32_t width;
    uint16_t channels;
    uint16_t word;
    uint32_t expected;
    size_t first;
    size_t i;
    int ok;

    if (af_mode == 5 && row == 1u && af_reads == 1u) {
        ++af_injected;
        return -1;
    }
    ok = TIFFReadScanline(tif, data, row, sample);
    if (ok != 1) { return ok; }
    ++af_reads;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
    first = (size_t)row * width * channels;
    for (i = 0u; i < (size_t)width * channels; ++i) {
        word = ta_cases[af_fixture].bits == 16
            ? ((uint16_t const *)data)[i] : ((unsigned char const *)data)[i];
        expected = ta_cases[af_fixture].stored[first + i];
        af_matched += word == expected;
    }
    return ok;
}

static int
af_rgba_read(TIFF *tif, uint32_t width, uint32_t height, uint32_t *raster,
             int orientation, int stop)
{
    ++af_rgba;
    return TIFFReadRGBAImageOriented(tif, width, height, raster,
                                     orientation, stop);
}
#define SIXEL_CMS_PROFILE_BACKEND_BUILTIN af_local_0
#define SIXEL_CMS_PROFILE_BACKEND_COLORSYNC af_local_1
#define SIXEL_CMS_PROFILE_BACKEND_LCMS2 af_local_2
#define SIXEL_CMS_PROFILE_BACKEND_NONE af_local_3
#define g_sixel_cms_engine af_local_4
#define g_sixel_cms_engine_mutex af_local_5
#define g_sixel_cms_engine_mutex_once af_local_6
#define g_sixel_cms_engine_mutex_ready af_local_7
#define g_sixel_cms_engine_once af_local_8
#define g_sixel_loader_libtiff_vtbl af_local_9
#define load_tiff af_local_10
#define load_with_libtiff af_local_11
#define loader_can_try_libtiff af_local_12
#define sixel_cdef_clone af_local_13
#define sixel_cdef_finite af_local_14
#define sixel_cdef_free af_local_15
#define sixel_cdef_from_xyz af_local_16
#define sixel_cdef_icc af_local_17
#define sixel_cdef_matrix af_local_18
#define sixel_cdef_name af_local_19
#define sixel_cdef_output_ready af_local_20
#define sixel_cdef_parse af_local_21
#define sixel_cdef_pcs_scale af_local_22
#define sixel_cdef_rgb af_local_23
#define sixel_cdef_to_xyz af_local_24
#define sixel_cms_allocate_profile af_local_25
#define sixel_cms_apply_backend af_local_26
#define sixel_cms_apply_matrix3 af_local_27
#define sixel_cms_ascii_case_equal af_local_28
#define sixel_cms_build_builtin_a2b_order af_local_29
#define sixel_cms_build_intent_order af_local_30
#define sixel_cms_build_rgb_to_xyz_matrix af_local_31
#define sixel_cms_build_rgb_to_xyz_matrix_internal af_local_32
#define sixel_cms_builtin_apply_cmyk_f32_intent af_local_33
#define sixel_cms_builtin_apply_cmyk_u16_intent af_local_34
#define sixel_cms_builtin_apply_cmyk_u8_intent af_local_35
#define sixel_cms_builtin_apply_gray_u8_intent af_local_36
#define sixel_cms_builtin_apply_rgb_f32_intent af_local_37
#define sixel_cms_builtin_apply_rgb_u8_intent af_local_38
#define sixel_cms_builtin_apply_src_to_xyz_intent af_local_39
#define sixel_cms_builtin_apply_xyz_to_dst_intent af_local_40
#define sixel_cms_builtin_convert_rgb8_to_rgbf32 af_local_41
#define sixel_cms_builtin_kind_channel_count af_local_42
#define sixel_cms_builtin_load_src_unit af_local_43
#define sixel_cms_builtin_profile_can_convert af_local_44
#define sixel_cms_builtin_profile_is_srgb af_local_45
#define sixel_cms_builtin_store_dst_unit af_local_46
#define sixel_cms_clamp_lab_ab af_local_47
#define sixel_cms_clamp_unit af_local_48
#define sixel_cms_close_profile af_local_49
#define sixel_cms_colorsync_create_transform_one af_local_50
#define sixel_cms_colorsync_intent_to_cfstring af_local_51
#define sixel_cms_colorsync_lab_units af_local_52
#define sixel_cms_colorsync_map_format af_local_53
#define sixel_cms_colorsync_profile_channels af_local_54
#define sixel_cms_colorsync_promote_source af_local_55
#define sixel_cms_colorsync_source_matches af_local_56
#define sixel_cms_convert_colorsync_linear af_local_57
#define sixel_cms_convert_profile_to_cielab af_local_58
#define sixel_cms_convert_profile_to_linearrgb af_local_59
#define sixel_cms_convert_profile_to_srgb af_local_60
#define sixel_cms_convert_rgbf32_gamma_to_linear af_local_61
#define sixel_cms_convert_to_cielab_with_profile_bytes af_local_62
#define sixel_cms_convert_to_linearrgb_with_profile_bytes af_local_63
#define sixel_cms_convert_to_srgb_with_profile_bytes af_local_64
#define sixel_cms_create_cielab_d50_profile af_local_65
#define sixel_cms_create_linear_srgb_profile af_local_66
#define sixel_cms_create_rgb_profile_from_gamma_chrm af_local_67
#define sixel_cms_create_rgb_profile_from_gammas_chrm af_local_68
#define sixel_cms_create_srgb_profile af_local_69
#define sixel_cms_create_transform af_local_70
#define sixel_cms_create_transform_builtin af_local_71
#define sixel_cms_create_transform_colorsync af_local_72
#define sixel_cms_create_transform_lcms2 af_local_73
#define sixel_cms_decode_srgb_unit af_local_74
#define sixel_cms_delete_transform af_local_75
#define sixel_cms_do_transform af_local_76
#define sixel_cms_do_transform_builtin af_local_77
#define sixel_cms_do_transform_builtin_device_to_device af_local_78
#define sixel_cms_do_transform_builtin_to_srgb af_local_79
#define sixel_cms_encode_srgb_unit af_local_80
#define sixel_cms_engine_from_string af_local_81
#define sixel_cms_engine_lock af_local_82
#define sixel_cms_engine_lock_init_once af_local_83
#define sixel_cms_engine_to_string af_local_84
#define sixel_cms_engine_unlock af_local_85
#define sixel_cms_float_rgb af_local_86
#define sixel_cms_float_source_xyz af_local_87
#define sixel_cms_get_color_space af_local_88
#define sixel_cms_get_engine af_local_89
#define sixel_cms_intent_contains af_local_90
#define sixel_cms_intent_name af_local_91
#define sixel_cms_intent_to_a2b_slot af_local_92
#define sixel_cms_lcms_input_units af_local_93
#define sixel_cms_map_format_lcms af_local_94
#define sixel_cms_matrix3_invert af_local_95
#define sixel_cms_matrix3_invert_internal af_local_96
#define sixel_cms_native_gamma af_local_97
#define sixel_cms_open_profile_from_mem af_local_98
#define sixel_cms_parse_icc_colorspace_signature af_local_99
#define sixel_cms_prepare_linear_gray af_local_100
#define sixel_cms_linear_unit_pixel taf_mixed_unit
#define sixel_cms_apply_linear_colorsync taf_mixed_apply
#define sixel_cms_prepare_linear_rgb af_local_101
#define sixel_cms_profile af_local_102
#define sixel_cms_profile_backend af_local_103
#define sixel_cms_profile_backend_t af_local_104
#define sixel_cms_profile_create_cielab_d50_builtin af_local_105
#define sixel_cms_profile_create_cielab_d50_colorsync af_local_106
#define sixel_cms_profile_create_cielab_d50_lcms2 af_local_107
#define sixel_cms_profile_create_rgb_gamma_chrm_builtin af_local_108
#define sixel_cms_profile_create_rgb_gamma_chrm_colorsync af_local_109
#define sixel_cms_profile_create_rgb_gamma_chrm_lcms2 af_local_110
#define sixel_cms_profile_create_srgb_builtin af_local_111
#define sixel_cms_profile_create_srgb_colorsync af_local_112
#define sixel_cms_profile_create_srgb_lcms2 af_local_113
#define sixel_cms_profile_open_builtin af_local_114
#define sixel_cms_profile_open_colorsync af_local_115
#define sixel_cms_profile_open_lcms2 af_local_116
#define sixel_cms_profile_t af_local_117
#define sixel_cms_resolve_engine af_local_118
#define sixel_cms_rgb_power_tag af_local_119
#define sixel_cms_rgb_set_tag af_local_120
#define sixel_cms_set_engine af_local_121
#define sixel_cms_srgb_to_xyz_d65 af_local_122
#define sixel_cms_srgb_unit_to_xyz_d50 af_local_123
#define sixel_cms_trace_gamma af_local_124
#define sixel_cms_trace_intent_order af_local_125
#define sixel_cms_transform af_local_126
#define sixel_cms_transform_engine af_local_127
#define sixel_cms_transform_t af_local_128
#define sixel_cms_xyz_d50_to_d65 af_local_129
#define sixel_cms_xyz_d50_to_srgb_unit af_local_130
#define sixel_cms_xyz_d65_to_d50 af_local_131
#define sixel_cms_xyz_to_srgb_d65 af_local_132
#define sixel_color_def af_local_133
#define sixel_color_def_t af_local_134
#define sixel_color_xform af_local_135
#define sixel_color_xform_t af_local_136
#define sixel_cxform_apply af_local_137
#define sixel_cxform_free af_local_138
#define sixel_cxform_new af_local_139
#define sixel_loader_libtiff_component af_local_140
#define sixel_loader_libtiff_component_t af_local_141
#define sixel_loader_libtiff_load af_local_142
#define sixel_loader_libtiff_name af_local_143
#define sixel_loader_libtiff_new af_local_144
#define sixel_loader_libtiff_placeholder af_local_145
#define sixel_loader_libtiff_placeholder_function af_local_146
#define sixel_loader_libtiff_predicate af_local_147
#define sixel_loader_libtiff_ref af_local_148
#define sixel_loader_libtiff_setopt af_local_149
#define sixel_loader_libtiff_unref af_local_150
#define tiff_clamp_lab_ab af_local_151
#define tiff_clamp_unit af_local_152
#define tiff_convert_embedded_icc_to_srgb af_local_153
#define tiff_convert_embedded_icc_to_srgb_nolcms af_local_155
#define tiff_decode_lab_u16_ab af_local_157
#define tiff_decode_lab_u16_l af_local_158
#define tiff_map_scanline_coordinates af_local_159
#define tiff_memory_chunk af_local_160
#define tiff_memory_chunk_t af_local_161
#define tiff_memory_close af_local_162
#define tiff_memory_map af_local_163
#define tiff_memory_read af_local_164
#define tiff_memory_seek af_local_165
#define tiff_memory_size af_local_166
#define tiff_memory_unmap af_local_167
#define tiff_memory_write af_local_168
#define tiff_normalize_lab_ab_sample af_local_169
#define tiff_normalize_lab_l_sample af_local_170
#define tiff_orientation_is_supported af_local_171
#define tiff_photometric_allows_embedded_icc af_local_172
#define tiff_try_load_high_precision af_local_173
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


#define malloc af_malloc
#define free af_free
#define sixel_icc_curve_device af_curve
#define ColorSyncTransformConvert af_cs
#define cmsDoTransform af_lcms
#include "src/cms.c"
#undef malloc
#undef free
#undef sixel_icc_curve_device
#undef ColorSyncTransformConvert
#undef cmsDoTransform
/* Observe rollback at the borrowed-profile boundary, before compaction. */
static int
af_linear(unsigned char *pixels, int width, int height, int format,
          sixel_cms_profile_t *profile)
{
    unsigned char before[12u * sizeof(float)];
    unsigned char before_alpha[8];
    size_t alpha_bytes;
    unsigned int alpha;
    size_t i;
    size_t channels;
    int ok;

    ++af_cms_calls;
    memcpy(before, pixels, sizeof(before));
    af_source_ok &= memcmp(before, ta_cases[af_fixture].source,
                           sizeof(before)) == 0;
    if (af_alpha == NULL) { return 0; }
    alpha_bytes = af_n * (size_t)(ta_cases[af_fixture].bits / 8);
    memcpy(before_alpha, af_alpha, alpha_bytes);
    channels = ta_cases[af_fixture].photo == 2 ? 4u : 2u;
    for (i = 0u; i < 4u; ++i) {
        alpha = ta_cases[af_fixture].bits == 16
            ? ((uint16_t const *)af_alpha)[i] : af_alpha[i];
        af_alpha_ok &= alpha == ta_cases[af_fixture].stored[
            i * channels + channels - 1u];
    }
    ok = af_local_59(pixels, width, height, format, profile);
    af_cms_result = ok;
    af_alpha_ok &= memcmp(before_alpha, af_alpha, alpha_bytes) == 0;
    af_rollback = ok || memcmp(before, pixels, sizeof(before)) == 0;
    return ok;
}
#undef sixel_cms_convert_profile_to_linearrgb
#define sixel_cms_convert_profile_to_linearrgb af_linear
/* A loader-owned ICC retry must fail compilation, even in fault tests. */
#define sixel_icc_apply_rgb_float32(...) SIXEL_LOADER_MUST_USE_CMS_FACADE
#define TIFFReadScanline af_read
#define TIFFReadRGBAImageOriented af_rgba_read
#define tiff_logluv_xyz_to_linear af_logluv_xyz
#define tiff_convert_samples_to_linear af_byte
#define tiff_try_load_integer_alpha af_load_alpha
#define tiff_read_separate_uint16 b1_namespace_4
#define tiff_publish_integer_alpha til_ns_9_0
#define tiff_integer_layout_t til_ns_9_1
#define tiff_scatter_integer_row til_ns_9_2
#define tiff_try_load_integer_layouts til_ns_9_3
#include "src/loader-libtiff.c"
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
af_capture(sixel_frame_t *frame, void *context)
{
    size_t i;
    size_t j;
    double scale;
    double allowance;
    double expected;
    double actual;
    uint32_t word;
    int fallback;
    int floating;
    int masked;

    (void)context;
    ++af_calls;
    fallback = af_phase == 1;
    floating = ta_cases[af_fixture].bits == 16 || !fallback ||
        (!ta_cases[af_fixture].opaque &&
         (ta_cases[af_fixture].associated || af_background));
    masked = !af_background && !ta_cases[af_fixture].opaque;
    af_frame_ok = frame->width * frame->height == 4 &&
        frame->pixelformat == (fallback ? (floating
            ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888)
            : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) &&
        frame->colorspace == (fallback ? SIXEL_COLORSPACE_GAMMA
                                      : SIXEL_COLORSPACE_LINEAR) &&
        (frame->transparent_mask_size == (masked ? 4u : 0u)) &&
        frame->alpha_zero_is_transparent == masked;
    if (!af_frame_ok) { return SIXEL_BAD_INPUT; }
    for (i = 0u; i < 4u; ++i) {
        if (masked && frame->transparent_mask[i] != (i == 0u)) {
            af_frame_ok = 0;
        }
        scale = 0.0;
        for (j = 0u; j < 3u; ++j) {
            expected = ta_cases[af_fixture].managed[af_background]
                [af_engine == 2 ? 0 : 1][i * 3u + j];
            scale = fmax(scale, fabs(expected));
        }
        allowance = scale * (af_engine == 4
            ? 0.00032384445817172027 : 0.000003814697265625);
        for (j = 0u; j < 3u; ++j) {
            if (!fallback) {
                expected = ta_cases[af_fixture].managed[af_background]
                    [af_engine == 2 ? 0 : 1][i * 3u + j];
                actual = ((float *)frame->pixels.u8ptr)[i * 3u + j];
                if (!isfinite(actual) || fabs(actual - expected) > allowance) {
                    af_frame_ok = 0;
                }
            } else if (floating) {
                memcpy(&word, frame->pixels.u8ptr +
                    (i * 3u + j) * sizeof(float), sizeof(word));
                if (word != ta_cases[af_fixture].fallback[0][af_background]
                    [i * 3u + j]) { af_frame_ok = 0; }
            } else {
                expected = ta_cases[af_fixture].bytes[i * 3u + j];
                if (frame->pixels.u8ptr[i * 3u + j] != expected) {
                    af_frame_ok = 0;
                }
            }
        }
    }
    if (!fallback) {
        memcpy(af_frame, frame->pixels.u8ptr, 12u * sizeof(float));
    }
    return af_frame_ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

static SIXELSTATUS
af_load(sixel_loader_component_t *component, sixel_chunk_t *chunk,
        int phase, int mode)
{
    SIXELSTATUS status;
    int cms;

    fprintf(stderr, "AFAULT|phase-begin=%d\n", phase);
    af_phase = phase;
    af_mode = mode;
    af_success = af_curve_calls = af_reject = af_calls = af_cms_calls = 0u;
    af_poison = af_allocated = af_freed = 0u;
    af_reads = af_matched = af_seen = af_rgba = 0u;
    af_segment = af_changed = af_remainder = af_frame_ok = 0;
    af_scratch = NULL;
    af_rollback = af_injected = 0;
    af_source_ok = af_alpha_ok = 1;
    af_cms_result = -1;
    af_alpha = NULL;
    af_color_allocated = 0;
    cms = 1;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &cms);
    if (SIXEL_FAILED(status)) { return status; }
    af_armed = 1;
    status = component->vtbl->load(component, chunk, af_capture, NULL);
    af_armed = 0;
    fprintf(stderr, "AFAULT|phase=%d|mode=%d|status=%x|calls=%zu|"
        "success=%zu|reject=%zu|segment=%d|changed=%d|remainder=%d|"
        "alloc=%zu|freed=%zu|cms=%zu|poison=%zu|"
        "read=%zu|matched=%zu|rgba=%zu|frame=%d|source=%d|alpha=%d|"
        "cms-result=%d|rollback=%d\n",
        phase, mode, status, af_calls, af_success, af_reject, af_segment,
        af_changed, af_remainder, af_allocated, af_freed,
        af_cms_calls, af_poison, af_reads, af_matched, af_rgba,
        af_frame_ok, af_source_ok, af_alpha_ok, af_cms_result, af_rollback);
    return status;
}

static int
af_run(char const *directory, int fixture, int engine, int mode,
       int background_on)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    void *object;
    SIXELSTATUS status;
    unsigned char original[4096];
    unsigned char background[3];
    float saved[12];
    size_t source_size;
    size_t expected_samples;
    char path[4096];
    int ok;
    int normal;
    int recovered;

    af_fixture = fixture;
    af_engine = engine;
    af_background = background_on;
    snprintf(path, sizeof(path), "%s/tiff-alpha-%s-%s.tif", directory,
        ta_cases[fixture].name, ta_cases[fixture].opaque ? "icc" : "o1-s1");
    /* Fault witnesses are the four-pixel normal and opaque rows. The
     * ten-pixel boundary rows belong to the source/composition observer. */
    af_n = (size_t)ta_cases[fixture].count;
    if (af_n != 4u) { return 0; }
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    source_size = 0u;
    ok = 0;
    af_local_121((sixel_cms_engine_t)af_engine);
    if ((int)af_local_89() != af_engine) { return 77; }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, af_image_malloc,
        af_image_calloc, af_image_realloc, af_image_free))) { goto cleanup; }
    if (SIXEL_FAILED(sixel_chunk_create_from_source(&chunk,
        path, 0, NULL, allocator))) { goto cleanup; }
    source_size = sixel_chunk_get_size(chunk);
    if (source_size > sizeof(original)) { goto cleanup; }
    memcpy(original, sixel_chunk_get_buffer(chunk), source_size);
    if (SIXEL_FAILED(af_local_144(allocator, &object))) { goto cleanup; }
    component = object;
    background[0] = 19u;
    background[1] = 43u;
    background[2] = 71u;
    if (af_background) {
        status = component->vtbl->setopt(component,
            SIXEL_LOADER_OPTION_BGCOLOR, background);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    }
    expected_samples = ta_cases[af_fixture].photo == 2 ? 16u : 8u;
    status = af_load(component, chunk, 0, 0);
    normal = status == SIXEL_OK && af_calls == 1u && af_frame_ok &&
        af_allocated == 1u && af_freed == 1u && af_scratch == NULL &&
        af_matched == expected_samples && af_cms_calls == 1u &&
        af_rgba == 0u && af_source_ok && af_alpha_ok &&
        af_success == af_n &&
        af_reads == (ta_cases[af_fixture].opaque ? 1u : 2u);
    memcpy(saved, af_frame, sizeof(saved));
    status = af_load(component, chunk, 1, mode);
    if (mode >= 4) {
        ok = SIXEL_FAILED(status) && af_calls == 0u && af_rgba == 0u &&
            af_injected == 1 && af_cms_calls == 0u &&
            af_reads == (mode == 5 ? 1u : 0u) &&
            af_matched == (mode == 5 ? expected_samples / 2u : 0u);
    } else {
        ok = status == SIXEL_OK && af_calls == 1u && af_frame_ok &&
            af_allocated == 1u && af_freed == 1u && af_scratch == NULL &&
            af_cms_calls == 1u && af_rollback && af_rgba == 0u &&
            af_matched == expected_samples && af_source_ok && af_alpha_ok;
        if (mode == 1) {
            ok &= af_success == 2u && af_reject > 0u &&
                  af_segment && af_changed && af_remainder;
        }
        if (mode == 3) {
            ok &= af_success == af_n && af_poison == 1u &&
                  af_segment && af_changed;
        }
    }
    status = af_load(component, chunk, 2, 0);
    recovered = status == SIXEL_OK && af_calls == 1u && af_frame_ok &&
        memcmp(af_frame, saved, sizeof(saved)) == 0 && af_allocated == 1u &&
        af_freed == 1u && af_scratch == NULL && af_source_ok && af_alpha_ok &&
        af_success == af_n && af_cms_calls == 1u &&
        af_rgba == 0u &&
        af_reads == (ta_cases[af_fixture].opaque ? 1u : 2u) &&
        af_matched == expected_samples;
    ok = ok && normal && recovered;
cleanup:
    if (ok && chunk != NULL) {
        ok = memcmp(original, sixel_chunk_get_buffer(chunk), source_size) == 0;
    }
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    printf("AFAULT|fixture=%d|engine=%d|mode=%d|background=%d|"
           "live=%zu|ok=%d\n", af_fixture, af_engine, mode,
           af_background, af_live, ok);
    return ok && af_live == 0u ? 1 : 0;
}
#endif

static int
af_test(int argc, char **argv, int photo, int engine, int mode, int bits)
{
#if HAVE_LIBTIFF
    int previous_policy;
    int previous_background;
    int association;
    int background;
    int fixture;
    int injected_mode;
    int offset;
    int ok;

    if (argc != 2) { return 2; }
    offset = bits == 16 ? 20 : 0;
    previous_policy = loader_transparent_policy();
    previous_background = loader_background_colorspace();
    sixel_helper_set_loader_transparent_policy(SIXEL_ALPHA_POLICY_COMPOSITE);
    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_GAMMA);
    ok = 1;
    if (mode == 0) {
        /* Six bounded source witnesses per engine: both associations with
         * and without background, then both opaque association controls.
         * ColorSync observes both real-prefix and finite-result rejection. */
        for (association = 0; ok == 1 && association < 2; ++association) {
            fixture = offset + photo * 2 + association;
            injected_mode = engine == 3 || (engine == 4 && association)
                ? 3 : 1;
            for (background = 0; ok == 1 && background < 2; ++background) {
                ok = af_run(argv[1], fixture, engine, injected_mode,
                            background);
            }
            if (ok == 1) {
                ok = af_run(argv[1], fixture + (bits == 16 ? 12 : 8),
                            engine, injected_mode, association);
            }
        }
    } else {
        injected_mode = mode == 1 ? 7 : mode == 2 ? 4 : mode == 3 ? 6 : 5;
        for (fixture = offset; ok == 1 && fixture < offset + 6;
             fixture += 2) {
            ok = af_run(argv[1], fixture, 2, injected_mode, 0);
        }
    }
    sixel_helper_set_loader_transparent_policy(previous_policy);
    sixel_helper_set_loader_background_colorspace(previous_background);
    return ok == 77 ? 77 : ok == 1 ? 0 : 1;
#else
    (void)argc; (void)argv; (void)photo; (void)engine; (void)mode;
    (void)bits;
    return 77;
#endif
}

int
tiff_alpha_fault(int argc, char **argv, int photo, int engine, int mode)
{
    return af_test(argc, argv, photo, engine, mode, 8);
}

int
tiff_alpha16_fault(int argc, char **argv, int photo, int engine, int mode)
{
    return af_test(argc, argv, photo, engine, mode, 16);
}
