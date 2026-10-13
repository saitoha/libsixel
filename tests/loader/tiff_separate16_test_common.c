/* SPDX-License-Identifier: MIT */
/* Include the actual CMS and adapter. Only dependency calls are replaced;
 * the conversion, selection, rollback and publication code stays unchanged.
 * Literal integer/F32 tables own source accuracy. Direct contiguous output
 * owns adapter consistency; it is not an independent CMM numeric oracle. */
#include "config.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/compat_stub.h"
#include "src/cms.h"
#include "src/icc-apply.h"
#include "src/frame-private.h"
#include "src/loader-libtiff.h"
#include "src/loader-common.h"
#include "src/chunk-factory.h"
#include "src/chunk-view.h"
#include "tiff_separate16_test_common.h"

#if HAVE_LIBTIFF
#include <tiffio.h>
#if defined(__APPLE__) && HAVE_COREGRAPHICS
#include <ColorSync/ColorSync.h>
#endif
#if HAVE_LCMS2
#include <lcms2.h>
#endif
#include "tests/data/color-numeric/tiff-separate16.h"

/* Six original pixels have two non-symmetric rows. Fault numbers 1-7 own
 * mandatory source failures; 8-13 own optional CMS transaction failures. */
static struct {
    int fixture, engine, mode, managed, loading, linear, excluded;
    int ok, calls, reads, rgba, cms_calls, applies, injected, curve_calls;
    int image_attempts, cms_attempts, cms_live, image_live, real_pixels;
    int changed, remainder, target, prefer, baseline, fallback;
    int gray_checked, output_checked, result;
    void *heap[64], *gray, *output;
    float reference[18];
    unsigned char *source;
} ts;

static int
ts_words(void const *data, uint32_t const *expected, size_t count)
{
    uint32_t word;
    size_t i;

    for (i = 0u; i < count; ++i) {
        memcpy(&word, (unsigned char const *)data + i * sizeof(float),
               sizeof(word));
        if (word != expected[i]) { return 0; }
    }
    return 1;
}

static void *
ts_image_malloc(size_t bytes)
{
    void *data;

    if (ts.loading && !ts.excluded) {
        ++ts.image_attempts;
        if ((ts.mode == 1 && ts.image_attempts == 1) ||
            (ts.mode == 2 && ts.image_attempts == 2)) {
            ++ts.injected;
            return NULL;
        }
    }
    data = malloc(bytes);
    ts.image_live += data != NULL;
    return data;
}

static void *
ts_image_calloc(size_t count, size_t size)
{
    void *data;

    data = calloc(count, size);
    ts.image_live += data != NULL;
    return data;
}

static void *
ts_image_realloc(void *old, size_t size)
{
    void *data;
    int empty;

    empty = old == NULL;
    data = realloc(old, size);
    ts.image_live += empty && data != NULL;
    return data;
}

static void
ts_image_free(void *data)
{
    ts.image_live -= data != NULL;
    free(data);
}

static void *
ts_malloc(size_t bytes)
{
    void *data;
    int i;
    int gray;

    gray = ts_cases[ts.fixture].photo != 2;
    if (ts.linear) {
        ++ts.cms_attempts;
        /* Only the shared helper's first two scratch allocations and the
         * subsequent target profile allocation are selected for failure. */
        if ((ts.mode == 8 && ts.cms_attempts == 1) ||
            (ts.mode == 9 && ts.cms_attempts == 1) ||
            (ts.mode == 10 && ts.cms_attempts == 2) ||
            (ts.mode == 11 && ts.cms_attempts == 2 + gray)) {
            ++ts.injected;
            return NULL;
        }
    }
    data = malloc(bytes);
    if (data == NULL) { return NULL; }
    if (ts.loading) {
        for (i = 0; i < 64 && ts.heap[i] != NULL; ++i) { }
        if (i == 64) { ts.ok = 0; } else { ts.heap[i] = data; }
        ++ts.cms_live;
    }
    if (ts.linear && gray && ts.cms_attempts == 1) {
        ts.ok &= bytes == 6u * sizeof(float);
        ts.gray = data;
    }
    if (ts.linear && ts.cms_attempts == 1 + gray) {
        ts.ok &= bytes == 18u * sizeof(float) && data != ts.source;
        /* A broken profile gate can request a Gray-sized allocation for
         * an RGB source. Reject the observation without overrunning that
         * unexpected allocation in the observer itself. */
        if (bytes != 18u * sizeof(float)) { return data; }
        ts.output = data;
        /* A finite marker reveals unwritten tails after native prefix work.
         * LCMS has a void API; its prefix control poisons one later value
         * so the real shared finite-result check supplies the false result. */
        for (i = 0; i < 18; ++i) { ((float *)data)[i] = 0.125f; }
    }
    return data;
}

static void
ts_free(void *data)
{
    size_t i;
    int slot;

    if (data != NULL && data == ts.gray) {
        for (i = 0u; i < 6u; ++i) {
            ts.ok &= ts_words((float *)data + i,
                ts_cases[ts.fixture].source + i * 3u, 1u);
        }
        ++ts.gray_checked;
        ts.gray = NULL;
    }
    if (data != NULL && data == ts.output) {
        if (ts.mode == 12) {
            ts.ok &= memcmp(data, ts.reference, 6u * sizeof(float)) == 0;
            ts.changed = memcmp(data, ts.source, 6u * sizeof(float)) != 0;
            ts.remainder = 1;
            for (i = 6u; i < 18u; ++i) {
                if (ts.engine == 3 && i == 17u) { continue; }
                ts.remainder &= ((float *)data)[i] == 0.125f;
            }
        }
        ++ts.output_checked;
        ts.output = NULL;
    }
    if (data != NULL && ts.loading) {
        for (slot = 0; slot < 64 && ts.heap[slot] != data; ++slot) { }
        if (slot == 64) { ts.ok = 0; } else { ts.heap[slot] = NULL; }
        --ts.cms_live;
    }
    free(data);
}

static void *
ts_calloc(size_t count, size_t size)
{
    void *data;
    int i;

    data = calloc(count, size);
    if (data != NULL && ts.loading) {
        for (i = 0; i < 64 && ts.heap[i] != NULL; ++i) { }
        if (i == 64) { ts.ok = 0; } else { ts.heap[i] = data; }
        ++ts.cms_live;
    }
    return data;
}

/* Preserve printf argument checking through this variadic trace wrapper. */
static void ts_trace(char const *topic, char const *format, ...)
    SIXEL_ATTRIBUTE_FORMAT(SIXEL_PRINTF_ARCHETYPE, 2, 3);

static void
ts_trace(char const *topic, char const *format, ...)
{
    va_list args;
    char line[1024];
    char expected[80];

    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (ts.loading && strstr(line, "LSXCMS2|event=apply|") != NULL) {
        ++ts.applies;
        snprintf(expected, sizeof(expected), "|engine=%s|",
            ts.engine == 2 ? "builtin" : ts.engine == 3 ? "lcms2"
                                                          : "colorsync");
        ts.ok &= strstr(line, expected) != NULL;
        snprintf(expected, sizeof(expected), "|src=%d|dst=%d|",
            ts_cases[ts.fixture].photo == 2 ? SIXEL_CMS_PIXELFORMAT_RGB_F32
                                          : SIXEL_CMS_PIXELFORMAT_GRAY_F32,
            SIXEL_CMS_PIXELFORMAT_RGB_F32);
        ts.ok &= strstr(line, expected) != NULL;
        /* Builtin has no separate native output callback. This trace comes
         * after the actual complete apply and before shared publication. */
        if (ts.engine == 2 && ts.mode == 13 && ts.output != NULL) {
            ((float *)ts.output)[17] = NAN;
            ++ts.injected;
        }
    }
    sixel_trace_topic_message(topic, "%s", line);
}

static int
ts_curve(sixel_icc_curve_t const *curve, double x, int inverse, double *y)
{
    int ok;
    int channels;

    if (!ts.linear) { return sixel_icc_curve_device(curve, x, inverse, y); }
    channels = ts_cases[ts.fixture].photo == 2 ? 3 : 1;
    if (ts.mode == 12 && ts.curve_calls >= 2 * channels) {
        ++ts.injected;
        return 0;
    }
    ok = sixel_icc_curve_device(curve, x, inverse, y);
    if (ok) { ++ts.curve_calls; ts.real_pixels = ts.curve_calls / channels; }
    return ok;
}

#if defined(__APPLE__) && HAVE_COREGRAPHICS
static bool
ts_cs(ColorSyncTransformRef tr, size_t w, size_t h,
    void *dst, ColorSyncDataDepth dd, ColorSyncDataLayout dl, size_t ds,
    void const *src, ColorSyncDataDepth sd, ColorSyncDataLayout sl,
    size_t ss, CFDictionaryRef options)
{
    bool ok;

    ok = ColorSyncTransformConvert(tr, ts.mode == 12 ? 2u : w,
        ts.mode == 12 ? 1u : h, dst, dd, dl, ds, src, sd, sl, ss, options);
    ts.real_pixels += ok ? (ts.mode == 12 ? 2 : (int)(w * h)) : 0;
    ts.ok &= src != dst && sd == kColorSync32BitFloat &&
             dd == kColorSync32BitFloat;
    if (ts.mode == 12) { ++ts.injected; return false; }
    if (ok && ts.mode == 13) { ((float *)dst)[17] = NAN; ++ts.injected; }
    return ok;
}
#endif

#if HAVE_LCMS2
static void
ts_lcms(cmsHTRANSFORM tr, void const *src, void *dst, cmsUInt32Number n)
{
    cmsDoTransform(tr, src, dst, ts.mode == 12 ? 2u : n);
    ts.real_pixels += ts.mode == 12 ? 2 : (int)n;
    ts.ok &= src != dst;
    if (ts.mode == 12 || ts.mode == 13) {
        ((float *)dst)[17] = NAN;
        ++ts.injected;
    }
}
#endif

static tmsize_t
ts_size(TIFF *tif)
{
    tmsize_t size;

    size = TIFFScanlineSize(tif);
    if (ts.excluded) { return size; }
    if (ts.mode >= 3 && ts.mode <= 5) {
        ++ts.injected;
        return ts.mode == 3 ? 0 : ts.mode == 4 ? -1 : size - 1;
    }
    return size;
}

static int
ts_read(TIFF *tif, void *data, uint32_t row, uint16_t sample)
{
    size_t i;
    size_t index;
    size_t channels;
    size_t count;
    int separate;
    int ok;

    ++ts.reads;
    if ((ts.mode == 6 && ts.reads == 2) ||
        (ts.mode == 7 && ts.reads == 4)) { ++ts.injected; return -1; }
    ok = TIFFReadScanline(tif, data, row, sample);
    if (ts.excluded) { ts.ok = 0; return ok; }
    channels = ts_cases[ts.fixture].photo == 2 ? 3u : 1u;
    separate = ts_cases[ts.fixture].planar == 2;
    ts.ok &= row < 2u && sample < channels && ok == 1;
    ts.ok &= ts.reads == (int)(row + (separate ? sample * 2u : 0u)) + 1;
    count = separate ? 3u : 3u * channels;
    for (i = 0u; ok == 1 && i < count; ++i) {
        index = separate ? ((size_t)row * 3u + i) * channels + sample
                         : (size_t)row * 3u * channels + i;
        ts.ok &= ((uint16_t const *)data)[i] ==
                 ts_cases[ts.fixture].original[index];
    }
    return ok;
}

static int
ts_rgba(TIFF *tif, uint32_t w, uint32_t h, uint32_t *raster,
        int orientation, int stop)
{
    ++ts.rgba;
    return TIFFReadRGBAImageOriented(tif, w, h, raster, orientation, stop);
}

static int
ts_target_format(void)
{
    /* Intercept the getter actually called by the adapter. Intercepting
     * only colorspace/prefer getters in this translation unit misses the
     * target-format helper compiled in the shared library. */
    return ts.prefer ? SIXEL_PIXELFORMAT_RGB888
         : ts.target == SIXEL_COLORSPACE_OKLAB
         ? SIXEL_PIXELFORMAT_OKLABFLOAT32
         : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
}

static int
ts_tiled(TIFF *tif)
{
    /* A control load asks the adapter to decline direct scanlines. LibTIFF
     * itself still sees the real directory when its RGBA path is invoked. */
    return ts.baseline ? 1 : TIFFIsTiled(tif);
}
#define SIXEL_CMS_PROFILE_BACKEND_BUILTIN ts_local_0
#define SIXEL_CMS_PROFILE_BACKEND_COLORSYNC ts_local_1
#define SIXEL_CMS_PROFILE_BACKEND_LCMS2 ts_local_2
#define SIXEL_CMS_PROFILE_BACKEND_NONE ts_local_3
#define g_sixel_cms_engine ts_local_4
#define g_sixel_cms_engine_mutex ts_local_5
#define g_sixel_cms_engine_mutex_once ts_local_6
#define g_sixel_cms_engine_mutex_ready ts_local_7
#define g_sixel_cms_engine_once ts_local_8
#define g_sixel_loader_libtiff_vtbl ts_local_9
#define load_tiff ts_local_10
#define load_with_libtiff ts_local_11
#define loader_can_try_libtiff ts_local_12
#define sixel_cdef_clone ts_local_13
#define sixel_cdef_export_icc ts_local_13_export
#define sixel_cdef_finite ts_local_14
#define sixel_cdef_free ts_local_15
#define sixel_cdef_from_xyz ts_local_16
#define sixel_cdef_icc ts_local_17
#define sixel_cdef_matrix ts_local_18
#define sixel_cdef_name ts_local_19
#define sixel_cdef_named_name ts_local_19_identity
#define sixel_named_colors ts_local_19_registry
#define sixel_cdef_output_ready ts_local_20
#define sixel_cdef_parse ts_local_21
#define sixel_cdef_pcs_scale ts_local_22
#define sixel_cdef_rgb ts_local_23
#define sixel_cdef_to_xyz ts_local_24
#define sixel_cms_allocate_profile ts_local_25
#define sixel_cms_apply_backend ts_local_26
#define sixel_cms_apply_matrix3 ts_local_27
#define sixel_cms_ascii_case_equal ts_local_28
#define sixel_cms_build_builtin_a2b_order ts_local_29
#define sixel_cms_build_intent_order ts_local_30
#define sixel_cms_build_rgb_to_xyz_matrix ts_local_31
#define sixel_cms_build_rgb_to_xyz_matrix_internal ts_local_32
#define sixel_cms_builtin_apply_cmyk_f32_intent ts_local_33
#define sixel_cms_builtin_apply_cmyk_u16_intent ts_local_34
#define sixel_cms_builtin_apply_cmyk_u8_intent ts_local_35
#define sixel_cms_builtin_apply_gray_u8_intent ts_local_36
#define sixel_cms_builtin_apply_rgb_f32_intent ts_local_37
#define sixel_cms_builtin_apply_rgb_u8_intent ts_local_38
#define sixel_cms_builtin_apply_src_to_xyz_intent ts_local_39
#define sixel_cms_builtin_apply_xyz_to_dst_intent ts_local_40
#define sixel_cms_builtin_convert_rgb8_to_rgbf32 ts_local_41
#define sixel_cms_builtin_kind_channel_count ts_local_42
#define sixel_cms_builtin_load_src_unit ts_local_43
#define sixel_cms_builtin_profile_can_convert ts_local_44
#define sixel_cms_builtin_profile_is_srgb ts_local_45
#define sixel_cms_builtin_store_dst_unit ts_local_46
#define sixel_cms_clamp_lab_ab ts_local_47
#define sixel_cms_clamp_unit ts_local_48
#define sixel_cms_close_profile ts_local_49
#define sixel_cms_colorsync_create_transform_one ts_local_50
#define sixel_cms_colorsync_intent_to_cfstring ts_local_51
#define sixel_cms_colorsync_lab_units ts_local_52
#define sixel_cms_colorsync_map_format ts_local_53
#define sixel_cms_colorsync_profile_channels ts_local_54
#define sixel_cms_colorsync_promote_source ts_local_55
#define sixel_cms_colorsync_source_matches ts_local_56
#define sixel_cms_convert_colorsync_linear ts_local_57
#define sixel_cms_convert_profile_to_cielab ts_local_58
#define sixel_cms_convert_profile_to_linearrgb ts_local_59
#define sixel_cms_convert_profile_to_srgb ts_local_60
#define sixel_cms_convert_rgbf32_gamma_to_linear ts_local_61
#define sixel_cms_convert_to_cielab_with_profile_bytes ts_local_62
#define sixel_cms_convert_to_linearrgb_with_profile_bytes ts_local_63
#define sixel_cms_convert_to_srgb_with_profile_bytes ts_local_64
#define sixel_cms_create_cielab_d50_profile ts_local_65
#define sixel_cms_create_linear_srgb_profile ts_local_66
#define sixel_cms_create_rgb_profile_from_gamma_chrm ts_local_67
#define sixel_cms_create_rgb_profile_from_gammas_chrm ts_local_68
#define sixel_cms_cicp_name ts_cicp_name
#define sixel_cms_create_named_rgb_profile ts_named_profile
#define sixel_cms_create_srgb_profile ts_local_69
#define sixel_cms_create_transform ts_local_70
#define sixel_cms_create_transform_builtin ts_local_71
#define sixel_cms_create_transform_colorsync ts_local_72
#define sixel_cms_create_transform_lcms2 ts_local_73
#define sixel_cms_decode_srgb_unit ts_local_74
#define sixel_cms_delete_transform ts_local_75
#define sixel_cms_do_transform ts_local_76
#define sixel_cms_do_transform_builtin ts_local_77
#define sixel_cms_do_transform_builtin_device_to_device ts_local_78
#define sixel_cms_do_transform_builtin_to_srgb ts_local_79
#define sixel_cms_encode_srgb_unit ts_local_80
#define sixel_cms_engine_from_string ts_local_81
#define sixel_cms_engine_lock ts_local_82
#define sixel_cms_engine_lock_init_once ts_local_83
#define sixel_cms_engine_to_string ts_local_84
#define sixel_cms_engine_unlock ts_local_85
#define sixel_cms_float_rgb ts_local_86
#define sixel_cms_float_source_xyz ts_local_87
#define sixel_cms_get_color_space ts_local_88
#define sixel_cms_get_engine ts_local_89
#define sixel_cms_intent_contains ts_local_90
#define sixel_cms_intent_name ts_local_91
#define sixel_cms_intent_to_a2b_slot ts_local_92
#define sixel_cms_lcms_input_units ts_local_93
#define sixel_cms_map_format_lcms ts_local_94
#define sixel_cms_matrix3_invert ts_local_95
#define sixel_cms_matrix3_invert_internal ts_local_96
#define sixel_cms_native_gamma ts_local_97
#define sixel_cms_open_profile_from_mem ts_local_98
#define sixel_cms_parse_icc_colorspace_signature ts_local_99
#define sixel_cms_prepare_linear_gray ts_local_100
#define sixel_cms_linear_unit_pixel tsf_mixed_unit
#define sixel_cms_apply_linear_colorsync tsf_mixed_apply
#define sixel_cms_prepare_linear_rgb ts_local_101
#define sixel_cms_profile ts_local_102
#define sixel_cms_profile_backend ts_local_103
#define sixel_cms_profile_backend_t ts_local_104
#define sixel_cms_profile_create_cielab_d50_builtin ts_local_105
#define sixel_cms_profile_create_cielab_d50_colorsync ts_local_106
#define sixel_cms_profile_create_cielab_d50_lcms2 ts_local_107
#define sixel_cms_profile_create_rgb_gamma_chrm_builtin ts_local_108
#define sixel_cms_profile_create_rgb_gamma_chrm_colorsync ts_local_109
#define sixel_cms_profile_create_rgb_gamma_chrm_lcms2 ts_local_110
#define sixel_cms_profile_create_srgb_builtin ts_local_111
#define sixel_cms_profile_create_srgb_colorsync ts_local_112
#define sixel_cms_profile_create_srgb_lcms2 ts_local_113
#define sixel_cms_profile_open_builtin ts_local_114
#define sixel_cms_profile_open_colorsync ts_local_115
#define sixel_cms_profile_open_lcms2 ts_local_116
#define sixel_cms_profile_t ts_local_117
#define sixel_cms_resolve_engine ts_local_118
#define sixel_cms_rgb_power_tag ts_local_119
#define sixel_cms_rgb_set_tag ts_local_120
#define sixel_cms_set_engine ts_local_121
#define sixel_cms_srgb_to_xyz_d65 ts_local_122
#define sixel_cms_srgb_unit_to_xyz_d50 ts_local_123
#define sixel_cms_trace_gamma ts_local_124
#define sixel_cms_trace_intent_order ts_local_125
#define sixel_cms_transform ts_local_126
#define sixel_cms_transform_engine ts_local_127
#define sixel_cms_transform_t ts_local_128
#define sixel_cms_xyz_d50_to_d65 ts_local_129
#define sixel_cms_xyz_d50_to_srgb_unit ts_local_130
#define sixel_cms_xyz_d65_to_d50 ts_local_131
#define sixel_cms_xyz_to_srgb_d65 ts_local_132
#define sixel_color_def ts_local_133
#define sixel_color_def_t ts_local_134
#define sixel_color_xform ts_local_135
#define sixel_color_xform_t ts_local_136
#define sixel_cxform_apply ts_local_137
#define sixel_cxform_free ts_local_138
#define sixel_cxform_new ts_local_139
#define sixel_loader_libtiff_component ts_local_140
#define sixel_loader_libtiff_component_t ts_local_141
#define sixel_loader_libtiff_load ts_local_142
#define sixel_loader_libtiff_name ts_local_143
#define sixel_loader_libtiff_new ts_local_144
#define sixel_loader_libtiff_placeholder ts_local_145
#define sixel_loader_libtiff_placeholder_function ts_local_146
#define sixel_loader_libtiff_predicate ts_local_147
#define sixel_loader_libtiff_ref ts_local_148
#define sixel_loader_libtiff_setopt ts_local_149
#define sixel_loader_libtiff_unref ts_local_150
#define tiff_clamp_lab_ab ts_local_151
#define tiff_clamp_unit ts_local_152
#define tiff_convert_embedded_icc_to_srgb ts_local_153
#define tiff_convert_embedded_icc_to_srgb_nolcms ts_local_155
#define tiff_decode_lab_u16_ab ts_local_157
#define tiff_decode_lab_u16_l ts_local_158
#define tiff_map_scanline_coordinates ts_local_159
#define tiff_memory_chunk ts_local_160
#define tiff_memory_chunk_t ts_local_161
#define tiff_memory_close ts_local_162
#define tiff_memory_map ts_local_163
#define tiff_memory_read ts_local_164
#define tiff_memory_seek ts_local_165
#define tiff_memory_size ts_local_166
#define tiff_memory_unmap ts_local_167
#define tiff_memory_write ts_local_168
#define tiff_normalize_lab_ab_sample ts_local_169
#define tiff_normalize_lab_l_sample ts_local_170
#define tiff_orientation_is_supported ts_local_171
#define tiff_photometric_allows_embedded_icc ts_local_172
#define tiff_try_load_high_precision ts_local_173
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


#define malloc ts_malloc
#define calloc ts_calloc
#define free ts_free
#define sixel_icc_curve_device ts_curve
#define ColorSyncTransformConvert ts_cs
#define cmsDoTransform ts_lcms
#define sixel_trace_topic_message ts_trace
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
#undef calloc
#undef free
#undef sixel_icc_curve_device
#undef ColorSyncTransformConvert
#undef cmsDoTransform
/* The source profile is already checked by the adapter. Observe the shared
 * helper's input and its complete rollback, including Gray-shaped storage. */
static int
ts_linear(unsigned char *pixels, int width, int height, int format,
          sixel_cms_profile_t *profile)
{
    unsigned char before[18u * sizeof(float)];
    int ok;

    ++ts.cms_calls;
    ts.ok &= width == 3 && height == 2 &&
             format == SIXEL_PIXELFORMAT_RGBFLOAT32;
    ts.ok &= ts_words(pixels, ts_cases[ts.fixture].source, 18u);
    memcpy(before, pixels, sizeof(before));
    ts.source = pixels;
    ts.linear = 1;
    ok = ts_local_59(pixels, width, height, format, profile);
    ts.linear = 0;
    ts.result = ok;
    ts.ok &= ok || memcmp(before, pixels, sizeof(before)) == 0;
    ts.ok &= ts.gray == NULL && ts.output == NULL;
    return ok;
}
#undef sixel_cms_convert_profile_to_linearrgb
#define sixel_cms_convert_profile_to_linearrgb ts_linear
#define TIFFReadScanline ts_read
#define TIFFScanlineSize ts_size
#define TIFFReadRGBAImageOriented ts_rgba
#define TIFFIsTiled ts_tiled
#define loader_cms_target_pixelformat ts_target_format
#define tiff_logluv_xyz_to_linear ts_logluv
#define tiff_convert_samples_to_linear ts_samples
#define tiff_try_load_integer_alpha ts_alpha
#define tiff_read_separate_uint16 ts_separate
#define tiff_publish_integer_alpha til_ns_2_0
#define tiff_integer_layout_t til_ns_2_1
#define tiff_scatter_integer_row til_ns_2_2
#define tiff_try_load_integer_layouts til_ns_2_3
#define tiff_lab8_ranges tlx_copy_8_ranges
#define tiff_lab8_to_linear tlx_copy_8_linear
#define tiff_convert_cmyk_to_linear tiff_separate16_ink_linear
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
#undef loader_cms_target_pixelformat
#undef TIFFIsTiled
#undef TIFFReadScanline
#undef TIFFScanlineSize
#undef TIFFReadRGBAImageOriented
#undef sixel_trace_topic_message
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
#undef sixel_cdef_named_name
#undef sixel_named_colors
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
#undef sixel_cms_convert_to_linearrgb_with_profile_bytes
static SIXELSTATUS
ts_capture(sixel_frame_t *frame, void *context)
{
    int reference;

    reference = context != NULL;
    ++ts.calls;
    ts.ok &= frame->width == 3 && frame->height == 2 &&
        frame->transparent_mask_size == 0u &&
        frame->alpha_zero_is_transparent == 0 &&
        frame->pixelformat == (ts.fallback ? SIXEL_PIXELFORMAT_RGBFLOAT32
                            : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) &&
        frame->colorspace == (ts.fallback ? SIXEL_COLORSPACE_GAMMA
                                         : SIXEL_COLORSPACE_LINEAR);
    if (!ts.ok) { return SIXEL_BAD_INPUT; }
    if (reference) {
        memcpy(ts.reference, frame->pixels.u8ptr, sizeof(ts.reference));
    } else if (ts.fallback) {
        ts.ok &= ts_words(frame->pixels.u8ptr,
                          ts_cases[ts.fixture].source, 18u);
    } else {
        ts.ok &= memcmp(frame->pixels.u8ptr, ts.reference,
                        sizeof(ts.reference)) == 0;
    }
    return ts.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

/* The original contiguous16 adapter is an independent route to the same
 * CMM. Orient its complete result using the literal source's coordinates. */
static int
ts_reference(char const *directory)
{
    sixel_loader_component_t *component;
    sixel_factory_t *factory;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    SIXELSTATUS status;
    void *object;
    void *service;
    float saved[18];
    char path[4096];
    int index;
    int x;
    int y;
    int dx;
    int dy;
    int enabled;

    component = NULL;
    factory = NULL;
    allocator = NULL;
    chunk = NULL;
    object = NULL;
    service = NULL;
    enabled = 1;
    index = ts_cases[ts.fixture].photo == 2 ? 9 :
            ts_cases[ts.fixture].photo == 1 ? 10 : 11;
    snprintf(path, sizeof(path), "%s/tiff-separate16-%s.tif", directory,
             ts_cases[index].name);
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    /* Construct the ordinary library component through its exported
     * factory contract, including DLL builds with private constructors. */
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    factory = service;
    status = factory->vtbl->create(factory, "loader/libtiff", allocator,
                                   &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &enabled);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    ts.calls = 0;
    ts.fallback = 0;
    status = component->vtbl->load(component, chunk, ts_capture, &enabled);
    ts.ok &= status == SIXEL_OK && ts.calls == 1;
    memcpy(saved, ts.reference, sizeof(saved));
    for (y = 0; y < 2; ++y) {
        for (x = 0; x < 3; ++x) {
            dx = ts_cases[ts.fixture].orientation == 2 ||
                 ts_cases[ts.fixture].orientation == 3 ? 2 - x : x;
            dy = ts_cases[ts.fixture].orientation == 3 ||
                 ts_cases[ts.fixture].orientation == 4 ? 1 - y : y;
            memcpy(ts.reference + (dy * 3 + dx) * 3,
                   saved + (y * 3 + x) * 3, 3u * sizeof(float));
        }
    }
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (factory != NULL) { factory->vtbl->unref(factory); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status) && ts.ok;
}

static int
ts_phase(sixel_loader_component_t *component, sixel_chunk_t *chunk,
         int mode)
{
    SIXELSTATUS status;
    int live;
    int channels;
    int mandatory;
    int converted;

    live = ts.image_live;
    channels = ts_cases[ts.fixture].photo == 2 ? 3 : 1;
    mandatory = mode >= 1 && mode <= 7;
    converted = ts.managed && ts_cases[ts.fixture].matched && mode == 0;
    ts.mode = mode;
    ts.fallback = !converted;
    ts.calls = ts.reads = ts.rgba = ts.cms_calls = ts.applies = 0;
    ts.injected = ts.curve_calls = ts.real_pixels = 0;
    ts.image_attempts = ts.cms_attempts = 0;
    ts.changed = ts.remainder = ts.gray_checked = ts.output_checked = 0;
    ts.result = -1;
    ts.loading = 1;
    status = component->vtbl->load(component, chunk, ts_capture, NULL);
    ts.loading = 0;
    ts.ok &= ts.image_live == live && ts.cms_live == 0 && ts.rgba == 0;
    if (mandatory) {
        ts.ok &= SIXEL_FAILED(status) && ts.calls == 0 &&
                 ts.cms_calls == 0 && ts.applies == 0 && ts.injected == 1;
        ts.ok &= ts.reads == (mode == 6 ? 2 : mode == 7 ? 4 : 0);
    } else {
        ts.ok &= status == SIXEL_OK && ts.calls == 1 &&
            ts.reads == (ts_cases[ts.fixture].planar == 2 ? channels * 2 : 2);
        if (ts.managed && ts_cases[ts.fixture].matched) {
            ts.ok &= ts.cms_calls == 1 && ts.result == converted;
            ts.ok &= ts.applies == (mode >= 8 && mode <= 11 ? 0 : 1);
            ts.ok &= ts.gray_checked == (channels == 1 && mode != 9);
            ts.ok &= ts.output_checked == (mode != 8 && mode != 9 &&
                                          mode != 10);
        } else {
            ts.ok &= ts.cms_calls == 0 && ts.applies == 0;
        }
        if (mode != 0) { ts.ok &= ts.injected > 0; }
        if (mode == 12) {
            ts.ok &= ts.real_pixels == 2 && ts.changed && ts.remainder;
        }
        if (mode == 13) { ts.ok &= ts.real_pixels == 6; }
    }
    fprintf(stderr, "TS16|fixture=%d|mode=%d|engine=%d|status=%x|"
        "calls=%d|reads=%d|rgba=%d|cms=%d|applies=%d|real=%d|fault=%d|"
        "image=%d|cms-live=%d|ok=%d\n", ts.fixture, mode, ts.engine,
        status, ts.calls, ts.reads, ts.rgba, ts.cms_calls, ts.applies,
        ts.real_pixels, ts.injected, ts.image_live - live, ts.cms_live, ts.ok);
    return ts.ok;
}

static int
ts_run(char const *directory, int fixture, int engine, int managed,
       int mode, int target)
{
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    void *object;
    unsigned char original[4096];
    size_t bytes;
    SIXELSTATUS status;
    char path[4096];
    int ok;

    memset(&ts, 0, sizeof(ts));
    ts.fixture = fixture;
    ts.engine = engine;
    ts.managed = managed;
    ts.target = target == 1 ? SIXEL_COLORSPACE_OKLAB : 0;
    ts.prefer = target == 2;
    ts.ok = 1;
    ts_local_121((sixel_cms_engine_t)engine);
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    if ((int)ts_local_89() != engine) { return 77; }
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    ok = 0;
    if (managed && ts_cases[fixture].matched && !ts_reference(directory)) {
        goto cleanup;
    }
    snprintf(path, sizeof(path), "%s/tiff-separate16-%s.tif", directory,
             ts_cases[fixture].name);
    status = sixel_allocator_new(&allocator, ts_image_malloc,
        ts_image_calloc, ts_image_realloc, ts_image_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&chunk, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    bytes = sixel_chunk_get_size(chunk);
    if (bytes > sizeof(original)) { goto cleanup; }
    memcpy(original, sixel_chunk_get_buffer(chunk), bytes);
    status = ts_local_144(allocator, &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &managed);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    /* Exercise the identical borrowed chunk and component before, during
     * and after the selected failure. No retry can hide a changed source. */
    ok = ts_phase(component, chunk, 0);
    ok &= memcmp(original, sixel_chunk_get_buffer(chunk), bytes) == 0;
    if (mode != 0) {
        ok &= ts_phase(component, chunk, mode);
        ok &= memcmp(original, sixel_chunk_get_buffer(chunk), bytes) == 0;
    }
    ok &= ts_phase(component, chunk, 0);
    ok &= memcmp(original, sixel_chunk_get_buffer(chunk), bytes) == 0;
cleanup:
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    return ok && ts.image_live == 0 && ts.cms_live == 0 ? 0 : 1;
}

static struct {
    int width, height, format, colorspace, alpha;
    size_t bytes, mask_size;
    unsigned char *pixels, *mask;
} ts_legacy;

static SIXELSTATUS
ts_legacy_capture(sixel_frame_t *frame, void *context)
{
    size_t bytes;

    (void)context;
    ++ts.calls;
    bytes = (size_t)frame->width * (size_t)frame->height *
        (SIXEL_PIXELFORMAT_IS_FLOAT32(frame->pixelformat) ?
         3u * sizeof(float) :
         (size_t)sixel_helper_compute_depth(frame->pixelformat));
    if (ts.baseline) {
        ts_legacy.width = frame->width;
        ts_legacy.height = frame->height;
        ts_legacy.format = frame->pixelformat;
        ts_legacy.colorspace = frame->colorspace;
        ts_legacy.alpha = frame->alpha_zero_is_transparent;
        ts_legacy.bytes = bytes;
        ts_legacy.mask_size = frame->transparent_mask_size;
        ts_legacy.pixels = malloc(bytes);
        ts_legacy.mask = malloc(frame->transparent_mask_size + 1u);
        if (ts_legacy.pixels == NULL || ts_legacy.mask == NULL) {
            ts.ok = 0;
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(ts_legacy.pixels, frame->pixels.u8ptr, bytes);
        if (frame->transparent_mask_size != 0u) {
            memcpy(ts_legacy.mask, frame->transparent_mask,
                   frame->transparent_mask_size);
        }
    } else {
        ts.ok &= ts_legacy.width == frame->width &&
            ts_legacy.height == frame->height &&
            ts_legacy.format == frame->pixelformat &&
            ts_legacy.colorspace == frame->colorspace &&
            ts_legacy.alpha == frame->alpha_zero_is_transparent &&
            ts_legacy.bytes == bytes &&
            ts_legacy.mask_size == frame->transparent_mask_size;
        if (!ts.ok) { return SIXEL_BAD_INPUT; }
        ts.ok &= memcmp(ts_legacy.pixels, frame->pixels.u8ptr, bytes) == 0;
        if (frame->transparent_mask_size != 0u) {
            ts.ok &= memcmp(ts_legacy.mask, frame->transparent_mask,
                            frame->transparent_mask_size) == 0;
        }
    }
    return ts.ok ? SIXEL_OK : SIXEL_BAD_INPUT;
}

/* Reuse tracked byte/alpha/tile/float sources. Only directory tag values
 * change in private memory for orientation, separate float and extras.
 * The live RGBA route is the outcome oracle even when a runtime accepts a
 * valid tile: the observation never hardcodes rejection of that tile. */
static int
ts_excluded(char const *directory, int variant)
{
    static char const *const names[] = {
        "tiff-legacy-rgb8-planar.tif",
        "../inputs/formats/rgb-tiled.tiff",
        "tiff-float-rgb-le.tif",
        "tiff-legacy-rgb8-planar-alpha.tif",
        "tiff-separate16-rgb16-planar.tif"
    };
    sixel_allocator_t *allocator;
    sixel_chunk_t *file;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    unsigned char *data;
    void *object;
    size_t bytes;
    size_t offset;
    unsigned int entries;
    unsigned int tag;
    int changed;
    int i;
    int enabled;
    int calls;
    int rgba;
    int ok;
    char path[4096];
    SIXELSTATUS status;
    SIXELSTATUS expected;

    memset(&ts, 0, sizeof(ts));
    memset(&ts_legacy, 0, sizeof(ts_legacy));
    ts.ok = 1;
    ts.excluded = 1;
    allocator = NULL;
    file = NULL;
    chunk = NULL;
    component = NULL;
    object = NULL;
    data = NULL;
    enabled = 0;
    ok = 0;
    snprintf(path, sizeof(path), "%s/%s", directory,
             names[variant < 4 ? variant : 4]);
    status = sixel_allocator_new(&allocator, ts_image_malloc,
        ts_image_calloc, ts_image_realloc, ts_image_free);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = sixel_chunk_create_from_source(&file, path, 0, NULL, allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    bytes = sixel_chunk_get_size(file);
    data = malloc(bytes);
    if (data == NULL) { goto cleanup; }
    memcpy(data, sixel_chunk_get_buffer(file), bytes);
    if (variant == 2 || variant >= 4) {
        /* These literal witnesses use a little-endian IFD at byte eight. */
        if (bytes < 10u || memcmp(data, "II*\0\10\0\0\0", 8u) != 0) {
            goto cleanup;
        }
        entries = (unsigned int)data[8] | ((unsigned int)data[9] << 8);
        changed = 0;
        for (i = 0; i < (int)entries; ++i) {
            offset = 10u + (size_t)i * 12u;
            if (offset + 12u > bytes) { goto cleanup; }
            tag = (unsigned int)data[offset] |
                  ((unsigned int)data[offset + 1u] << 8);
            if ((variant == 2 && tag == 284u) ||
                (variant >= 4 && variant <= 7 && tag == 274u)) {
                data[offset + 8u] = (unsigned char)(variant == 2 ? 2 :
                                                    variant + 1);
                ++changed;
            }
            if (variant == 8 && tag == 339u) {
                /* One associated extra channel makes this exact RGB16
                 * source ineligible. Its live decoder decides acceptance. */
                data[offset] = 82u;
                memset(data + offset + 4u, 0, 8u);
                data[offset + 4u] = 1u;
                data[offset + 8u] = 1u;
                ++changed;
            }
        }
        if (changed != 1) { goto cleanup; }
    }
    status = sixel_chunk_create_from_memory(&chunk, data, bytes, path,
                                             allocator);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    status = ts_local_144(allocator, &object);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    component = object;
    status = component->vtbl->setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS, &enabled);
    if (SIXEL_FAILED(status)) { goto cleanup; }
    ts.baseline = 1;
    ts.loading = 1;
    expected = component->vtbl->load(component, chunk, ts_legacy_capture,
                                      NULL);
    ts.loading = 0;
    calls = ts.calls;
    rgba = ts.rgba;
    ts.ok &= calls <= 1 && ts.reads == 0 && ts.applies == 0;
    for (i = 0; i < 2; ++i) {
        ts.calls = ts.rgba = ts.reads = 0;
        ts.baseline = 0;
        ts.loading = 1;
        status = component->vtbl->load(component, chunk, ts_legacy_capture,
                                       NULL);
        ts.loading = 0;
        ts.ok &= status == expected && ts.calls == calls &&
                 ts.rgba == rgba && ts.reads == 0 && ts.applies == 0 &&
                 memcmp(data, sixel_chunk_get_buffer(chunk), bytes) == 0;
    }
    fprintf(stderr, "TS16|excluded=%d|live-status=%x|calls=%d|ok=%d\n",
            variant, expected, calls, ts.ok);
    ok = ts.ok;
cleanup:
    free(ts_legacy.pixels);
    free(ts_legacy.mask);
    if (component != NULL) { component->vtbl->unref(component); }
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    if (file != NULL) { file->vtbl->unref(file); }
    sixel_allocator_unref(allocator);
    free(data);
    return ok && ts.image_live == 0 && ts.cms_live == 0 ? 0 : 1;
}
#endif

int
tiff_separate16_test(int argc, char **argv, int observation)
{
#if HAVE_LIBTIFF
    static int const first[] = {0, 4, 5, 6, 8, 7};
    int i;
    int result;
    int engine;

    if (argc != 2) { return 1; }
    engine = observation == 7 || observation == 25 ? 3 :
             observation == 8 || observation == 26 ? 4 : 2;
    if (observation < 6) {
        return ts_run(argv[1], first[observation], 2, 0, 0, 0);
    }
    if (observation >= 6 && observation <= 8) {
        for (i = 0; i < 9; ++i) {
            if (i >= 1 && i <= 3) { continue; }
            result = ts_run(argv[1], i, engine, 1, 0, 0);
            if (result != 0) { return result; }
        }
        return 0;
    }
    if (observation == 9) {
        result = ts_run(argv[1], 1, 2, 1, 0, 0);
        return result != 0 ? result : ts_run(argv[1], 12, 2, 1, 0, 0);
    }
    if (observation == 10) { return ts_run(argv[1], 13, 2, 1, 0, 0); }
    if (observation == 11 || observation == 12) {
        return ts_run(argv[1], observation - 9, 2, 1, 0, 0);
    }
    if (observation >= 13 && observation <= 19) {
        return ts_run(argv[1], 7, 2, 1, observation - 12, 0);
    }
    if (observation >= 20 && observation <= 23) {
        return ts_run(argv[1], observation == 21 || observation == 22
            ? 4 : 0, 2, 1, observation - 12, 0);
    }
    if (observation >= 24 && observation <= 27) {
        result = ts_run(argv[1], 0, engine, 1,
                        observation == 27 ? 13 : 12, 0);
        return result != 0 ? result : ts_run(argv[1], 4, engine, 1,
                        observation == 27 ? 13 : 12, 0);
    }
    if (observation == 28) {
        for (i = 1; i <= 2; ++i) {
            result = ts_run(argv[1], 0, 2, 1, 0, i);
            if (result != 0) { return result; }
        }
        return 0;
    }
    if (observation == 29) {
        /* Separate8, tiles, alpha and orientations now have their own
         * original-source layout tests. Retain only the still-delegated
         * separate float and incompatible extra-sample witnesses here. */
        for (i = 2; i <= 8; i += 6) {
            result = ts_excluded(argv[1], i);
            if (result != 0) { return result; }
        }
        return 0;
    }
    return 1;
#else
    (void)argc;
    (void)argv;
    (void)observation;
    return 77;
#endif
}
