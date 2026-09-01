/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <sixel.h>
#include <string.h>

#include "cms.h"
#include "dither-interframe-method.h"
#include "loader-common.h"
#include "options-registry.h"
#include "palette-common-cover.h"
#include "palette-heckbert.h"
#include "palette-kcenter.h"
#include "palette-kmeans.h"
#include "palette-kmedoids.h"

#define SIXEL_REGISTRY_ARRAY_LENGTH(array_) \
    (sizeof(array_) / sizeof((array_)[0]))

#define SIXEL_REGISTRY_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_) \
    }

#define SIXEL_REGISTRY_FREE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_FREE, NULL, 0u \
    }

enum {
    SIXEL_DEQUANTIZE_BASE_NONE = 0,
    SIXEL_DEQUANTIZE_BASE_K_UNDITHER,
    SIXEL_DEQUANTIZE_BASE_LSO_UNDITHER,
    SIXEL_DEQUANTIZE_BASE_SELECTIVE_BLUR
};

static sixel_option_value_schema_t const g_dequantize_values[] = {
    { "none", SIXEL_DEQUANTIZE_NONE, 0u },
    { "k_undither", SIXEL_DEQUANTIZE_K_UNDITHER, 0u },
    { "lso_undither", SIXEL_OPTION_DEQUANTIZE_LSO_BASE, 0u },
    { "selective_blur", SIXEL_DEQUANTIZE_SELECTIVE_BLUR, 0u }
};

enum {
    SIXEL_DIFFUSION_BASE_AUTO = 0,
    SIXEL_DIFFUSION_BASE_NONE,
    SIXEL_DIFFUSION_BASE_FS,
    SIXEL_DIFFUSION_BASE_ATKINSON,
    SIXEL_DIFFUSION_BASE_JAJUNI,
    SIXEL_DIFFUSION_BASE_STUCKI,
    SIXEL_DIFFUSION_BASE_BURKES,
    SIXEL_DIFFUSION_BASE_SIERRA,
    SIXEL_DIFFUSION_BASE_A_DITHER,
    SIXEL_DIFFUSION_BASE_X_DITHER,
    SIXEL_DIFFUSION_BASE_BLUENOISE,
    SIXEL_DIFFUSION_BASE_LSO2,
    SIXEL_DIFFUSION_BASE_INTERFRAME,
    SIXEL_DIFFUSION_BASE_STBN
};

static sixel_option_value_schema_t const g_diffusion_values[] = {
    { "auto", SIXEL_DIFFUSE_AUTO, 0u },
    { "none", SIXEL_DIFFUSE_NONE, 0u },
    { "fs", SIXEL_DIFFUSE_FS, 0u },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON, 0u },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI, 0u },
    { "stucki", SIXEL_DIFFUSE_STUCKI, 0u },
    { "burkes", SIXEL_DIFFUSE_BURKES, 0u },
    { "sierra", SIXEL_DIFFUSE_SIERRA1, 1u },
    { "a_dither", SIXEL_DIFFUSE_A_DITHER, 0u },
    { "x_dither", SIXEL_DIFFUSE_X_DITHER, 0u },
    { "bluenoise", SIXEL_DIFFUSE_BLUENOISE_DITHER, 6u },
    { "lso2", SIXEL_DIFFUSE_LSO2, 0u },
    { "interframe", SIXEL_DIFFUSE_INTERFRAME, 1u },
    { "stbn", SIXEL_DIFFUSE_INTERFRAME, 3u }
};

enum {
    SIXEL_QUANTIZE_BASE_AUTO = 0,
    SIXEL_QUANTIZE_BASE_HECKBERT,
    SIXEL_QUANTIZE_BASE_KMEANS,
    SIXEL_QUANTIZE_BASE_MEDOIDS,
    SIXEL_QUANTIZE_BASE_CENTER
};

static sixel_option_value_schema_t const g_quantize_values[] = {
    { "auto", SIXEL_QUANTIZE_MODEL_AUTO, 0u },
    { "heckbert", SIXEL_QUANTIZE_MODEL_MEDIANCUT, 1u },
    { "kmeans", SIXEL_QUANTIZE_MODEL_KMEANS, 17u },
    { "medoids", SIXEL_QUANTIZE_MODEL_KMEDOIDS, 17u },
    { "center", SIXEL_QUANTIZE_MODEL_KCENTER, 20u }
};

enum {
    SIXEL_LOOKUP_BASE_AUTO = 0,
    SIXEL_LOOKUP_BASE_5BIT,
    SIXEL_LOOKUP_BASE_6BIT,
    SIXEL_LOOKUP_BASE_NONE,
    SIXEL_LOOKUP_BASE_CERTLUT,
    SIXEL_LOOKUP_BASE_EYTZINGER,
    SIXEL_LOOKUP_BASE_FHEDT,
    SIXEL_LOOKUP_BASE_VPTREE,
    SIXEL_LOOKUP_BASE_RBC,
    SIXEL_LOOKUP_BASE_MAHALANOBIS
};

static sixel_option_value_schema_t const g_lookup_values[] = {
    { "auto", SIXEL_LUT_POLICY_AUTO, 0u },
    { "5bit", SIXEL_LUT_POLICY_5BIT, 0u },
    { "6bit", SIXEL_LUT_POLICY_6BIT, 0u },
    { "none", SIXEL_LUT_POLICY_NONE, 0u },
    { "certlut", SIXEL_LUT_POLICY_CERTLUT, 0u },
    { "eytzinger", SIXEL_LUT_POLICY_EYTZINGER, 0u },
    { "fhedt", SIXEL_LUT_POLICY_FHEDT, 0u },
    { "vptree", SIXEL_LUT_POLICY_VPTREE, 0u },
    { "rbc", SIXEL_LUT_POLICY_RBC, 0u },
    { "mahalanobis", SIXEL_LUT_POLICY_MAHALANOBIS, 0u }
};

enum {
    SIXEL_LOADER_VALUE_LIBPNG = 0,
    SIXEL_LOADER_VALUE_LIBJPEG,
    SIXEL_LOADER_VALUE_LIBWEBP,
    SIXEL_LOADER_VALUE_LIBTIFF,
    SIXEL_LOADER_VALUE_LIBRSVG,
    SIXEL_LOADER_VALUE_BUILTIN,
    SIXEL_LOADER_VALUE_WIC,
    SIXEL_LOADER_VALUE_COREGRAPHICS,
    SIXEL_LOADER_VALUE_GDK_PIXBUF2,
    SIXEL_LOADER_VALUE_GD,
    SIXEL_LOADER_VALUE_QUICKLOOK,
    SIXEL_LOADER_VALUE_GNOME_THUMBNAILER
};

/*
 * Feature-disabled loaders are omitted from the base table.  Keep a second
 * enum for physical table indexes so base pointers remain correct without
 * changing the stable resolved values above.
 */
enum {
#if HAVE_LIBPNG
    SIXEL_LOADER_INDEX_LIBPNG,
#endif
#if HAVE_JPEG
    SIXEL_LOADER_INDEX_LIBJPEG,
#endif
#if HAVE_WEBP
    SIXEL_LOADER_INDEX_LIBWEBP,
#endif
#if HAVE_LIBTIFF
    SIXEL_LOADER_INDEX_LIBTIFF,
#endif
#if HAVE_LIBRSVG
    SIXEL_LOADER_INDEX_LIBRSVG,
#endif
    SIXEL_LOADER_INDEX_BUILTIN,
#if HAVE_WIC
    SIXEL_LOADER_INDEX_WIC,
#endif
#if HAVE_COREGRAPHICS
    SIXEL_LOADER_INDEX_COREGRAPHICS,
#endif
#if HAVE_GDK_PIXBUF2
    SIXEL_LOADER_INDEX_GDK_PIXBUF2,
#endif
#if HAVE_GD
    SIXEL_LOADER_INDEX_GD,
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    SIXEL_LOADER_INDEX_QUICKLOOK,
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    SIXEL_LOADER_INDEX_GNOME_THUMBNAILER,
#endif
    SIXEL_LOADER_INDEX_COUNT
};

static sixel_option_value_schema_t const g_loader_values[] = {
#if HAVE_LIBPNG
    { "libpng", SIXEL_LOADER_VALUE_LIBPNG, 0u },
#endif
#if HAVE_JPEG
    { "libjpeg", SIXEL_LOADER_VALUE_LIBJPEG, 0u },
#endif
#if HAVE_WEBP
    { "libwebp", SIXEL_LOADER_VALUE_LIBWEBP, 0u },
#endif
#if HAVE_LIBTIFF
    { "libtiff", SIXEL_LOADER_VALUE_LIBTIFF, 0u },
#endif
#if HAVE_LIBRSVG
    { "librsvg", SIXEL_LOADER_VALUE_LIBRSVG, 0u },
#endif
    { "builtin", SIXEL_LOADER_VALUE_BUILTIN, 0u },
#if HAVE_WIC
    { "wic", SIXEL_LOADER_VALUE_WIC, 0u },
#endif
#if HAVE_COREGRAPHICS
    { "coregraphics", SIXEL_LOADER_VALUE_COREGRAPHICS, 0u },
#endif
#if HAVE_GDK_PIXBUF2
    { "gdk-pixbuf2", SIXEL_LOADER_VALUE_GDK_PIXBUF2, 0u },
#endif
#if HAVE_GD
    { "gd", SIXEL_LOADER_VALUE_GD, 0u },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    { "quicklook", SIXEL_LOADER_VALUE_QUICKLOOK, 0u },
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    {
        "gnome-thumbnailer",
        SIXEL_LOADER_VALUE_GNOME_THUMBNAILER,
        0u
    },
#endif
};

static sixel_suboption_choice_t const g_dequantize_lso_variant_choices[] = {
    { "fs", SIXEL_DEQUANTIZE_LSO_UNDITHER_VFS },
    { "light", SIXEL_DEQUANTIZE_LSO_UNDITHER_VLIGHT }
};

static sixel_suboption_choice_t const g_stbn_source_choices[] = {
    { "hash", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH },
    { "mask", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK },
    { "pmj", SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ }
};

static sixel_suboption_choice_t const g_toggle_01_choices[] = {
    { "0", 0 },
    { "1", 1 }
};

static sixel_suboption_choice_t const g_diffusion_scan_choices[] = {
    { "auto", SIXEL_SCAN_AUTO },
    { "serpentine", SIXEL_SCAN_SERPENTINE },
    { "raster", SIXEL_SCAN_RASTER }
};

static sixel_suboption_choice_t const g_bluenoise_channel_choices[] = {
    { "mono", 0 },
    { "rgb", 1 }
};

static sixel_suboption_choice_t const g_bluenoise_size_choices[] = {
    { "64", 64 }
};

static sixel_suboption_choice_t const g_interframe_diffusion_choices[] = {
    { "auto", SIXEL_DIFFUSE_FS },
    { "none", SIXEL_DIFFUSE_NONE },
    { "fs", SIXEL_DIFFUSE_FS },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI },
    { "stucki", SIXEL_DIFFUSE_STUCKI },
    { "burkes", SIXEL_DIFFUSE_BURKES },
    { "sierra1", SIXEL_DIFFUSE_SIERRA1 },
    { "sierra2", SIXEL_DIFFUSE_SIERRA2 },
    { "sierra3", SIXEL_DIFFUSE_SIERRA3 }
};

static sixel_suboption_choice_t const g_sierra_variant_choices[] = {
    { "1", SIXEL_DIFFUSE_SIERRA1 },
    { "2", SIXEL_DIFFUSE_SIERRA2 },
    { "3", SIXEL_DIFFUSE_SIERRA3 }
};

static sixel_suboption_choice_t const g_kmeans_init_type_choices[] = {
    { "auto", SIXEL_PALETTE_KMEANS_INIT_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_INIT_NONE },
    { "pca", SIXEL_PALETTE_KMEANS_INIT_PCA }
};

static sixel_suboption_choice_t const g_kmeans_binning_choices[] = {
    { "auto", SIXEL_PALETTE_KMEANS_BINNING_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_BINNING_NONE },
    { "hard", SIXEL_PALETTE_KMEANS_BINNING_HARD },
    { "soft", SIXEL_PALETTE_KMEANS_BINNING_SOFT }
};

static sixel_suboption_choice_t const g_kmeans_mapping_choices[] = {
    { "uniform", SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM },
    { "srgb", SIXEL_PALETTE_KMEANS_MAPPING_SRGB }
};

static sixel_suboption_choice_t const g_kmeans_softdist_choices[] = {
    { "trilinear", SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR }
};

static sixel_suboption_choice_t const g_kmeans_feedback_choices[] = {
    { "off", SIXEL_PALETTE_KMEANS_FEEDBACK_OFF },
    { "on", SIXEL_PALETTE_KMEANS_FEEDBACK_ON }
};

static sixel_suboption_choice_t const g_kmeans_prune_choices[] = {
    { "auto", SIXEL_PALETTE_KMEANS_PRUNE_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_PRUNE_NONE },
    { "hamerly", SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY },
    { "elkan", SIXEL_PALETTE_KMEANS_PRUNE_ELKAN },
    { "yinyang", SIXEL_PALETTE_KMEANS_PRUNE_YINYANG }
};

static sixel_suboption_choice_t const g_kmedoids_algo_choices[] = {
    { "auto", SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO },
    { "pam", SIXEL_PALETTE_KMEDOIDS_ALGO_PAM },
    { "sample", SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA },
    { "random", SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS },
    { "bandit", SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM }
};

static sixel_suboption_choice_t const g_kcenter_algo_choices[] = {
    { "auto", SIXEL_PALETTE_KCENTER_ALGO_AUTO },
    { "fft", SIXEL_PALETTE_KCENTER_ALGO_FFT },
    { "swap", SIXEL_PALETTE_KCENTER_ALGO_SWAP },
    { "hybrid", SIXEL_PALETTE_KCENTER_ALGO_HYBRID }
};

static sixel_suboption_choice_t const g_kcenter_profile_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_PROFILE_LEGACY },
    { "speed", SIXEL_PALETTE_KCENTER_PROFILE_SPEED },
    { "balance", SIXEL_PALETTE_KCENTER_PROFILE_BALANCE },
    { "quality", SIXEL_PALETTE_KCENTER_PROFILE_QUALITY }
};

static sixel_suboption_choice_t const g_kcenter_auto_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY },
    { "adaptive", SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE }
};

static sixel_suboption_choice_t const g_kcenter_space_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY },
    { "perceptual", SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL }
};

static sixel_suboption_choice_t const g_kcenter_candidate_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY },
    { "hybrid", SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID }
};

static sixel_suboption_choice_t const g_kcenter_budget_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY },
    { "adaptive", SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE }
};

static sixel_suboption_choice_t const g_kcenter_swap_update_choices[] = {
    { "full", SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL },
    { "incremental", SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL }
};

static sixel_suboption_choice_t const g_palette_cover_choices[] = {
    { "off", SIXEL_PALETTE_COVER_OFF },
    { "corners", SIXEL_PALETTE_COVER_CORNERS },
    { "faces", SIXEL_PALETTE_COVER_FACES },
    { "edges", SIXEL_PALETTE_COVER_EDGES },
    { "all", SIXEL_PALETTE_COVER_EDGES },
    { "auto", SIXEL_PALETTE_COVER_AUTO },
    { "on", SIXEL_PALETTE_COVER_AUTO }
};

static sixel_suboption_choice_t const g_palette_cover_grow_choices[] = {
    { "off", 0 },
    { "on", 1 }
};

static sixel_suboption_choice_t const g_palette_cover_mode_choices[] = {
    { "hard", SIXEL_PALETTE_COVER_MODE_HARD },
    { "soft", SIXEL_PALETTE_COVER_MODE_SOFT }
};

static sixel_suboption_choice_t const g_quantize_merge_choices[] = {
    { "auto", SIXEL_FINAL_MERGE_AUTO },
    { "none", SIXEL_FINAL_MERGE_NONE },
    { "ward", SIXEL_FINAL_MERGE_WARD }
};

static sixel_suboption_choice_t const g_heckbert_profile_choices[] = {
    { "compat", SIXEL_HECKBERT_PROFILE_COMPAT },
    { "speed", SIXEL_HECKBERT_PROFILE_SPEED },
    { "quality", SIXEL_HECKBERT_PROFILE_QUALITY }
};

static sixel_suboption_choice_t const g_lookup_shared_choices[] = {
    { "0", 0 },
    { "1", 1 }
};

static sixel_suboption_choice_t const g_loader_cms_engine_choices[] = {
    { "none", SIXEL_CMS_ENGINE_NONE },
    { "auto", SIXEL_CMS_ENGINE_AUTO },
    { "builtin", SIXEL_CMS_ENGINE_BUILTIN },
    { "lcms2", SIXEL_CMS_ENGINE_LCMS2 },
    { "colorsync", SIXEL_CMS_ENGINE_COLORSYNC }
};

static sixel_suboption_choice_t const g_loader_bmp_info40_mode_choices[] = {
    { "auto", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO },
    { "windows", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS },
    { "os2", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2 }
};

static sixel_suboption_choice_t const g_loader_orientation_choices[] = {
    { "on", 1 },
    { "off", 0 }
};

/*
 * This is the sole authoritative suboption registry.  A NULL base pointer
 * means that the row is shared by every base value of the owning option.
 * Common rows keep orthogonal controls identical without copying definitions
 * into every quantizer or diffusion method.
 */
static sixel_suboption_key_t const g_suboptions[] = {
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        g_dequantize_values + SIXEL_DEQUANTIZE_BASE_LSO_UNDITHER,
        "variant", 'V', "SIXEL_DEQUANTIZE_LSO_VARIANT", NULL, NULL,
        g_dequantize_lso_variant_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        g_dequantize_values + SIXEL_DEQUANTIZE_BASE_SELECTIVE_BLUR,
        "threshold", 'T',
        "SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD", NULL, NULL),

    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "scan", 'N', "SIXEL_DITHER_SCAN", NULL, NULL,
        g_diffusion_scan_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_SIERRA,
        "variant", 'V', "SIXEL_DITHER_SIERRA_VARIANT", NULL, NULL,
        g_sierra_variant_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_INTERFRAME,
        "diffusion", 'D', SIXEL_DITHER_INTERFRAME_DIFFUSION_ENVVAR,
        NULL, NULL, g_interframe_diffusion_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "source", 'S', SIXEL_DITHER_STBN_SOURCE_ENVVAR,
        NULL, NULL, g_stbn_source_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "diffusion", 'D', SIXEL_DITHER_STBN_DIFFUSION_ENVVAR,
        NULL, NULL, g_interframe_diffusion_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "strength", 'T', SIXEL_DITHER_STBN_STRENGTH_ENVVAR,
        NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "motion_adapt", 'M', SIXEL_DITHER_STBN_MOTION_ADAPT_ENVVAR,
        NULL, NULL, g_toggle_01_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "scene_cut_reset", 'C',
        SIXEL_DITHER_STBN_SCENE_CUT_RESET_ENVVAR,
        NULL, NULL, g_toggle_01_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "scene_detect", 'E', SIXEL_DITHER_STBN_SCENE_DETECT_ENVVAR,
        NULL, NULL, g_toggle_01_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "alpha_guard", 'A', SIXEL_DITHER_STBN_ALPHA_GUARD_ENVVAR,
        NULL, NULL, g_toggle_01_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "perceptual_weight", 'P',
        SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT_ENVVAR,
        NULL, NULL, g_toggle_01_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "fastpath", 'F', SIXEL_DITHER_STBN_FASTPATH_ENVVAR,
        NULL, NULL, g_toggle_01_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "strength", 'T', "SIXEL_DITHER_BLUENOISE_STRENGTH",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "gradient_factor", 'G',
        "SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "phase", 'P', "SIXEL_DITHER_BLUENOISE_PHASE", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "seed", 'S', "SIXEL_DITHER_BLUENOISE_SEED", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "channel", 'C', "SIXEL_DITHER_BLUENOISE_CHANNEL", NULL, NULL,
        g_bluenoise_channel_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "size", 'Z', "SIXEL_DITHER_BLUENOISE_SIZE", NULL, NULL,
        g_bluenoise_size_choices),

    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "merge", 'G', "SIXEL_PALETTE_FINAL_MERGE", NULL, NULL,
        g_quantize_merge_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "merge_oversplit", 'O', "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "merge_lloyd", 'L',
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "cover", 'C', "SIXEL_PALETTE_COVER", NULL, NULL,
        g_palette_cover_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "cover_grow", 'V', "SIXEL_PALETTE_COVER_GROW", NULL, NULL,
        g_palette_cover_grow_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "cover_mode", 'W', "SIXEL_PALETTE_COVER_MODE", NULL, NULL,
        g_palette_cover_mode_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_HECKBERT,
        "profile", 'P', "SIXEL_PALETTE_HECKBERT_PROFILE", NULL, NULL,
        g_heckbert_profile_choices),

    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "inittype", 'I', "SIXEL_PALETTE_KMEANS_INITTYPE", NULL, NULL,
        g_kmeans_init_type_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "threshold", 'T', "SIXEL_PALETTE_KMEANS_THRESHOLD", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "binning", 'B', "SIXEL_PALETTE_KMEANS_BINNING", NULL, NULL,
        g_kmeans_binning_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "binbits", 'N', "SIXEL_PALETTE_KMEANS_BINBITS", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "mapping", 'M', "SIXEL_PALETTE_KMEANS_MAPPING", NULL, NULL,
        g_kmeans_mapping_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "softdist", 'D', "SIXEL_PALETTE_KMEANS_SOFTDIST", NULL, NULL,
        g_kmeans_softdist_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "autoratio", 'R', "SIXEL_PALETTE_KMEANS_AUTORATIO", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback", 'F', "SIXEL_PALETTE_KMEANS_FEEDBACK", NULL, NULL,
        g_kmeans_feedback_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "prune", 'P', "SIXEL_PALETTE_KMEANS_PRUNE", NULL, NULL,
        g_kmeans_prune_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "seed", 'S', "SIXEL_PALETTE_KMEANS_SEED", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "restarts", 'E', "SIXEL_PALETTE_KMEANS_RESTARTS", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "iter", 'A', "SIXEL_PALETTE_KMEANS_ITER", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "iter_max", 'X', "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "miniter", 'U', "SIXEL_PALETTE_KMEANS_MINITER", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "polish_iter", 'H', "SIXEL_PALETTE_KMEANS_POLISH_ITER",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback_slots", 'K', "SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback_interval", 'J',
        "SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL", NULL, NULL),

    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "algo", 'A', "SIXEL_PALETTE_KMEDOIDS_ALGO", NULL, NULL,
        g_kmedoids_algo_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "seed", 'S', "SIXEL_PALETTE_KMEDOIDS_SEED", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "iter", 'I', "SIXEL_PALETTE_KMEDOIDS_ITER", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "sample", 'M', "SIXEL_PALETTE_KMEDOIDS_SAMPLE", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clara_trials", 'T', "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clara_sample", 'K', "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clarans_local", 'J', "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clarans_neighbors", 'N',
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_iter", 'D', "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_candidates", 'E',
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_batch", 'X', "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "histbits", 'H', "SIXEL_PALETTE_KMEDOIDS_HISTBITS", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "point_budget", 'B', "SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "rare_keep", 'R', "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "prune_mass", 'U', "SIXEL_PALETTE_KMEDOIDS_PRUNE_MASS",
        NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "auction", 'Q', "SIXEL_PALETTE_KMEDOIDS_AUCTION", NULL, NULL,
        g_toggle_01_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "auction_shortlist", 'Y',
        "SIXEL_PALETTE_KMEDOIDS_AUCTION_SHORTLIST", NULL, NULL),

    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "algo", 'A', "SIXEL_PALETTE_KCENTER_ALGO", NULL, NULL,
        g_kcenter_algo_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "profile", 'P', "SIXEL_PALETTE_KCENTER_PROFILE", NULL, NULL,
        g_kcenter_profile_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "seed", 'S', "SIXEL_PALETTE_KCENTER_SEED", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "auto_policy", 'Q', "SIXEL_PALETTE_KCENTER_AUTO_POLICY",
        NULL, NULL, g_kcenter_auto_policy_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "auto_fft_threshold", 'F',
        "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "space_policy", 'E', "SIXEL_PALETTE_KCENTER_SPACE_POLICY",
        NULL, NULL, g_kcenter_space_policy_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "candidate_policy", 'Z',
        "SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY",
        NULL, NULL, g_kcenter_candidate_policy_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "restarts", 'X', "SIXEL_PALETTE_KCENTER_RESTARTS", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "init_seeds", 'N', "SIXEL_PALETTE_KCENTER_INIT_SEEDS", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "iter", 'I', "SIXEL_PALETTE_KCENTER_ITER", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "histbits", 'H', "SIXEL_PALETTE_KCENTER_HISTBITS", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "point_budget", 'B', "SIXEL_PALETTE_KCENTER_POINT_BUDGET",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "rare_keep", 'R', "SIXEL_PALETTE_KCENTER_RARE_KEEP", NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "prune_mass", 'U', "SIXEL_PALETTE_KCENTER_PRUNE_MASS", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "budget_policy", 'D', "SIXEL_PALETTE_KCENTER_BUDGET_POLICY",
        NULL, NULL, g_kcenter_budget_policy_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "budget_scale", 'Y', "SIXEL_PALETTE_KCENTER_BUDGET_SCALE",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_topk", 'K', "SIXEL_PALETTE_KCENTER_SWAP_TOPK", NULL, NULL),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_update", 'M', "SIXEL_PALETTE_KCENTER_SWAP_UPDATE",
        NULL, NULL, g_kcenter_swap_update_choices),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_patience", 'T', "SIXEL_PALETTE_KCENTER_SWAP_PATIENCE",
        NULL, NULL),
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_min_gain", 'J', "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN",
        NULL, NULL),

    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_5BIT,
        "shared_instance", 'S', "SIXEL_LOOKUP_5BIT_SHARED_INSTANCE",
        NULL, NULL, g_lookup_shared_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_6BIT,
        "shared_instance", 'S', "SIXEL_LOOKUP_6BIT_SHARED_INSTANCE",
        NULL, NULL, g_lookup_shared_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_CERTLUT,
        "shared_instance", 'S', "SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE",
        NULL, NULL, g_lookup_shared_choices),

#if HAVE_LIBPNG
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBPNG,
        "cms_engine", 'E', "SIXEL_LOADER_LIBPNG_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBPNG,
        "orientation", 'O', "SIXEL_LOADER_LIBPNG_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL, g_loader_orientation_choices),
#endif
#if HAVE_JPEG
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG,
        "cms_engine", 'E', "SIXEL_LOADER_LIBJPEG_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG,
        "orientation", 'O', "SIXEL_LOADER_LIBJPEG_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL, g_loader_orientation_choices),
#endif
#if HAVE_WEBP
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "cms_engine", 'E', "SIXEL_LOADER_LIBWEBP_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "orientation", 'O', "SIXEL_LOADER_LIBWEBP_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL, g_loader_orientation_choices),
#endif
#if HAVE_COREGRAPHICS
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_COREGRAPHICS,
        "orientation", 'O', "SIXEL_LOADER_COREGRAPHICS_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL, g_loader_orientation_choices),
#endif
#if HAVE_LIBTIFF
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBTIFF,
        "cms_engine", 'E', "SIXEL_LOADER_LIBTIFF_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices),
#endif
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "cms_engine", 'E', "SIXEL_LOADER_BUILTIN_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "orientation", 'O', "SIXEL_LOADER_BUILTIN_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL, g_loader_orientation_choices),
    SIXEL_REGISTRY_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "bmp_info40_mode", 'B', "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
        NULL, NULL, g_loader_bmp_info40_mode_choices),
#if HAVE_WIC
    SIXEL_REGISTRY_FREE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_WIC,
        "ico_minsize", 'I', "SIXEL_LOADER_WIC_ICO_MINSIZE", NULL,
        "SIXEL_LODER_WIC_ICO_MINSIZE"),
#endif
};

static sixel_option_argument_schema_t const g_options[] = {
    {
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        SIXEL_OPTFLAG_DEQUANTIZE,
        "dequantize",
        g_dequantize_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_dequantize_values)
    },
    {
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        SIXEL_OPTFLAG_DIFFUSION,
        "--diffusion",
        g_diffusion_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_diffusion_values)
    },
    {
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        SIXEL_OPTFLAG_QUANTIZE_MODEL,
        "--quantize-model",
        g_quantize_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_quantize_values)
    },
    {
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        SIXEL_OPTFLAG_LUT_POLICY,
        "--lookup-policy",
        g_lookup_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_lookup_values)
    },
    {
        SIXEL_OPTION_SCHEMA_LOADERS,
        SIXEL_OPTFLAG_LOADERS,
        "--loaders",
        g_loader_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_loader_values)
    }
};

static int
sixel_option_registry_key_applies(
    sixel_suboption_key_t const *key,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def)
{
    if (key == NULL || schema == NULL || base_def == NULL) {
        return 0;
    }
    if (key->option_id != schema->option_id) {
        return 0;
    }
    return key->base_def == NULL || key->base_def == base_def;
}

sixel_option_argument_schema_t const *
sixel_option_registry_get(sixel_option_schema_id_t option_id)
{
    size_t index;

    index = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_options)) {
        if (g_options[index].option_id == option_id) {
            return g_options + index;
        }
        ++index;
    }

    return NULL;
}

size_t
sixel_option_registry_suboption_count(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def)
{
    size_t index;
    size_t count;

    index = 0u;
    count = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (sixel_option_registry_key_applies(g_suboptions + index,
                                              schema,
                                              base_def)) {
            ++count;
        }
        ++index;
    }

    return count;
}

sixel_suboption_key_t const *
sixel_option_registry_suboption_at(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    size_t requested_index)
{
    size_t index;
    size_t matched_index;
    size_t common_count;
    size_t specific_count;
    size_t common_offset;
    int request_common;

    index = 0u;
    matched_index = 0u;
    common_count = 0u;
    specific_count = 0u;
    common_offset = 0u;
    request_common = 0;

    if (schema == NULL || base_def == NULL) {
        return NULL;
    }

    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (g_suboptions[index].option_id == schema->option_id) {
            if (g_suboptions[index].base_def == NULL) {
                ++common_count;
            } else if (g_suboptions[index].base_def == base_def) {
                ++specific_count;
            }
        }
        ++index;
    }

    common_offset = base_def->common_suboption_offset;
    if (common_offset > specific_count) {
        common_offset = specific_count;
    }
    if (requested_index >= common_offset &&
        requested_index < common_offset + common_count) {
        request_common = 1;
        requested_index -= common_offset;
    } else {
        if (requested_index >= common_offset + common_count) {
            requested_index -= common_count;
        }
    }

    index = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (g_suboptions[index].option_id == schema->option_id &&
            ((request_common && g_suboptions[index].base_def == NULL) ||
             (!request_common &&
              g_suboptions[index].base_def == base_def))) {
            if (matched_index == requested_index) {
                return g_suboptions + index;
            }
            ++matched_index;
        }
        ++index;
    }

    return NULL;
}

int
sixel_option_registry_validate(void)
{
    size_t option_index;
    size_t base_index;
    size_t key_index;
    size_t previous_index;
    size_t key_count;
    sixel_option_argument_schema_t const *schema;
    sixel_option_value_schema_t const *base_def;
    sixel_suboption_key_t const *key;
    sixel_suboption_key_t const *previous;

    option_index = 0u;
    base_index = 0u;
    key_index = 0u;
    previous_index = 0u;
    key_count = 0u;
    schema = NULL;
    base_def = NULL;
    key = NULL;
    previous = NULL;

    while (option_index < SIXEL_REGISTRY_ARRAY_LENGTH(g_options)) {
        schema = g_options + option_index;
        base_index = 0u;
        while (base_index < schema->value_count) {
            base_def = schema->values + base_index;
            key_count = sixel_option_registry_suboption_count(schema,
                                                               base_def);
            key_index = 0u;
            while (key_index < key_count) {
                key = sixel_option_registry_suboption_at(schema,
                                                         base_def,
                                                         key_index);
                if (key == NULL || key->name == NULL ||
                    key->name[0] == '\0' || key->env_name == NULL ||
                    key->env_name[0] == '\0' ||
                    key->short_name < 'A' || key->short_name > 'Z') {
                    return 0;
                }
                previous_index = 0u;
                while (previous_index < key_index) {
                    previous = sixel_option_registry_suboption_at(
                        schema,
                        base_def,
                        previous_index);
                    if (previous == NULL ||
                        strcmp(previous->name, key->name) == 0 ||
                        previous->short_name == key->short_name) {
                        return 0;
                    }
                    ++previous_index;
                }
                ++key_index;
            }
            ++base_index;
        }
        ++option_index;
    }

    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
