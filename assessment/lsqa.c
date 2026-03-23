/*
 * lsqa.c - Thin CLI wrapper around the libsixel assessment engine.
 *
 *  Pipeline overview:
 *
 *        +---------------+    +------------------+    +-----------------+
 *        |  Frame input  | -> |  Assessment API  | -> |  JSON emission  |
 *        +---------------+    +------------------+    +-----------------+
 *             |                         |                        |
 *             |                         |                        +--> stdout
 *             |                         +--> sixel_assessment_t   +--> files
 *             +--> libsixel loader
 */

/*
 * Guard the feature test macros so unity builds do not complain when the
 * build system already defines the same POSIX surface area globally.  The
 * macros remain visible when missing so mkstemp() and strerror_r() stay
 * declared.
 */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_MATH_H
# include <stddef.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#include <sixel.h>

#include "assessment.h"
#include "getopt_stub.h"
#include "cli.h"

#if defined(_WIN32)
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# if HAVE_IO_H
#  include <io.h>
# endif
#elif HAVE_UNISTD_H
# include <unistd.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#if defined(_WIN32)
#define SIXEL_PATH_SEP '\\'
#else
#define SIXEL_PATH_SEP '/'
#endif

/*
 * Exit codes distinguish usage errors, runtime failures, and quality
 * regressions so automated callers can react deterministically.
 */
enum {
    LSQA_EXIT_SUCCESS = 0,
    LSQA_EXIT_BAD_ARGS = 2,
    LSQA_EXIT_LOAD_FAILED = 3,
    LSQA_EXIT_RUNTIME_FAILED = 4,
    LSQA_EXIT_BASELINE_FAILED = 5
};

/*
 * Local safety shims avoid MSVC secure CRT warnings without pulling in the
 * libsixel-wide compat layer. Each helper mirrors a single libc routine and
 * falls back to a bounded copy where secure variants are unavailable.
 */
static char const *
lsqa_strerror(int errnum, char *buffer, size_t size)
{
#if defined(_MSC_VER)
    errno_t rc;

    if (buffer != NULL && size > 0) {
        rc = strerror_s(buffer, size, errnum);
        if (rc == 0) {
            return buffer;
        }
    }
    return "unknown error";
#elif defined(HAVE_STRERROR_R)
    if (buffer != NULL && size > 0) {
        if (strerror_r(errnum, buffer, size) == 0) {
            return buffer;
        }
    }
    return "unknown error";
#else
    (void)buffer;
    (void)size;
    return strerror(errnum);
#endif
}

static char *
lsqa_getenv_dup(char const *name)
{
#if defined(_MSC_VER) && defined(HAVE__DUPENV_S)
    char *value;
    size_t len;
    errno_t rc;

    value = NULL;
    len = 0u;
    rc = _dupenv_s(&value, &len, name);
    if (rc != 0) {
        if (value != NULL) {
            free(value);
        }
        return NULL;
    }
    return value;
#else
    char const *value;
    size_t len;
    char *copy;

    value = getenv(name);
    if (value == NULL) {
        return NULL;
    }
    len = strlen(value);
    copy = (char *)malloc(len + 1u);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, len + 1u);
    return copy;
#endif
}

static FILE *
lsqa_fopen_write(char const *path)
{
    /* fopen_s depends on errno_t, which is MSVC-specific. */
#if defined(HAVE_FOPEN_S) && defined(_MSC_VER)
    FILE *stream;
    errno_t rc;

    stream = NULL;
    rc = fopen_s(&stream, path, "w");
    if (rc != 0) {
        return NULL;
    }
    return stream;
#else
    return fopen(path, "w");
#endif
}

typedef struct LoaderCapture {
    sixel_frame_t *frame;
} LoaderCapture;

static SIXELSTATUS
capture_first_frame(sixel_frame_t *frame, void *context)
{
    LoaderCapture *capture;

    capture = (LoaderCapture *)context;
    if (capture->frame == NULL) {
        sixel_frame_ref(frame);
        capture->frame = frame;
    }
    return SIXEL_OK;
}

static int
load_frame(char const *path, sixel_allocator_t *allocator,
           char const *loader_order,
           sixel_frame_t **out_frame)
{
    LoaderCapture capture;
    SIXELSTATUS status;
    sixel_loader_t *loader;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_override;
    int finsecure;

    capture.frame = NULL;
    loader = NULL;
    fstatic = 1;
    fuse_palette = 0;
    reqcolors = SIXEL_PALETTE_MAX;
    loop_override = SIXEL_LOOP_DISABLE;
    finsecure = 0;

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_override);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &finsecure);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (loader_order != NULL && loader_order[0] != '\0') {
        status = sixel_loader_setopt(loader,
                                     SIXEL_LOADER_OPTION_LOADER_ORDER,
                                     loader_order);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &capture);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_load_file(loader,
                                    path,
                                    capture_first_frame);
    if (SIXEL_FAILED(status)) {
        goto error;
    }
    if (capture.frame == NULL) {
        sixel_helper_set_additional_message(
            "loader did not produce any frame.");
        status = SIXEL_RUNTIME_ERROR;
        goto error;
    }

    sixel_loader_unref(loader);
    *out_frame = capture.frame;
    return 0;

error:
    if (SIXEL_FAILED(status)) {
        char const *detail;

        detail = sixel_helper_get_additional_message();
        if (detail != NULL && detail[0] != '\0') {
            fprintf(stderr,
                    "libsixel loader failed for %s: %s (%s)\n",
                    path,
                    sixel_helper_format_error(status),
                    detail);
        } else {
            fprintf(stderr,
                    "libsixel loader failed for %s: %s\n",
                    path,
                    sixel_helper_format_error(status));
        }
    }
    sixel_loader_unref(loader);
    if (capture.frame != NULL) {
        sixel_frame_unref(capture.frame);
    }
    return -1;
}

typedef struct Metrics {
    float ms_ssim;
    float high_freq_out;
    float high_freq_ref;
    float high_freq_delta;
    float stripe_ref;
    float stripe_out;
    float stripe_rel;
    float band_run_rel;
    float band_grad_rel;
    float clip_l_ref;
    float clip_r_ref;
    float clip_g_ref;
    float clip_b_ref;
    float clip_l_out;
    float clip_r_out;
    float clip_g_out;
    float clip_b_out;
    float clip_l_rel;
    float clip_r_rel;
    float clip_g_rel;
    float clip_b_rel;
    float delta_chroma_mean;
    float delta_e00_mean;
    float gmsd_value;
    float psnr_y;
    unsigned int valid_mask;
} Metrics;

typedef struct MetricBinding {
    const char *name;
    size_t offset;
    int metric_id;
} MetricBinding;

static const MetricBinding g_metric_bindings[] = {
    {"MS-SSIM", offsetof(Metrics, ms_ssim),
     SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"HighFreqRatio_out", offsetof(Metrics, high_freq_out),
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT},
    {"HighFreqRatio_ref", offsetof(Metrics, high_freq_ref),
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF},
    {"HighFreqRatio_delta", offsetof(Metrics, high_freq_delta),
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA},
    {"StripeScore_ref", offsetof(Metrics, stripe_ref),
     SIXEL_ASSESSMENT_METRIC_STRIPE_REF},
    {"StripeScore_out", offsetof(Metrics, stripe_out),
     SIXEL_ASSESSMENT_METRIC_STRIPE_OUT},
    {"StripeScore_rel", offsetof(Metrics, stripe_rel),
     SIXEL_ASSESSMENT_METRIC_STRIPE_REL},
    {"BandingIndex_rel", offsetof(Metrics, band_run_rel),
     SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL},
    {"BandingIndex_grad_rel", offsetof(Metrics, band_grad_rel),
     SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL},
    {"ClipRate_L_ref", offsetof(Metrics, clip_l_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REF},
    {"ClipRate_R_ref", offsetof(Metrics, clip_r_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REF},
    {"ClipRate_G_ref", offsetof(Metrics, clip_g_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REF},
    {"ClipRate_B_ref", offsetof(Metrics, clip_b_ref),
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REF},
    {"ClipRate_L_out", offsetof(Metrics, clip_l_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT},
    {"ClipRate_R_out", offsetof(Metrics, clip_r_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT},
    {"ClipRate_G_out", offsetof(Metrics, clip_g_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT},
    {"ClipRate_B_out", offsetof(Metrics, clip_b_out),
     SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT},
    {"ClipRate_L_rel", offsetof(Metrics, clip_l_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REL},
    {"ClipRate_R_rel", offsetof(Metrics, clip_r_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REL},
    {"ClipRate_G_rel", offsetof(Metrics, clip_g_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REL},
    {"ClipRate_B_rel", offsetof(Metrics, clip_b_rel),
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REL},
    {"Δ Chroma_mean", offsetof(Metrics, delta_chroma_mean),
     SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA},
    {"Δ E00_mean", offsetof(Metrics, delta_e00_mean),
     SIXEL_ASSESSMENT_METRIC_DELTA_E00},
    {"GMSD", offsetof(Metrics, gmsd_value),
     SIXEL_ASSESSMENT_METRIC_GMSD},
    {"PSNR_Y", offsetof(Metrics, psnr_y),
     SIXEL_ASSESSMENT_METRIC_PSNR_Y},
};

typedef struct MetricSpec {
    const char *option;
    const char *json_key;
    int metric_id;
} MetricSpec;

static const MetricSpec sixel_metric_specs[] = {
    {"MS-SSIM", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"MS_SSIM", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"SSIM", "MS-SSIM", SIXEL_ASSESSMENT_METRIC_MS_SSIM},
    {"HIGHFREQ_OUT", "HighFreqRatio_out",
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_OUT},
    {"HIGHFREQ_REF", "HighFreqRatio_ref",
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_REF},
    {"HIGHFREQ_DELTA", "HighFreqRatio_delta",
     SIXEL_ASSESSMENT_METRIC_HIGH_FREQ_DELTA},
    {"STRIPE_REF", "StripeScore_ref",
     SIXEL_ASSESSMENT_METRIC_STRIPE_REF},
    {"STRIPE_OUT", "StripeScore_out",
     SIXEL_ASSESSMENT_METRIC_STRIPE_OUT},
    {"STRIPE_REL", "StripeScore_rel",
     SIXEL_ASSESSMENT_METRIC_STRIPE_REL},
    {"BANDING_REL", "BandingIndex_rel",
     SIXEL_ASSESSMENT_METRIC_BAND_RUN_REL},
    {"BANDING_GRAD_REL", "BandingIndex_grad_rel",
     SIXEL_ASSESSMENT_METRIC_BAND_GRAD_REL},
    {"CLIP_L_REF", "ClipRate_L_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REF},
    {"CLIP_R_REF", "ClipRate_R_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REF},
    {"CLIP_G_REF", "ClipRate_G_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REF},
    {"CLIP_B_REF", "ClipRate_B_ref",
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REF},
    {"CLIP_L_OUT", "ClipRate_L_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_L_OUT},
    {"CLIP_R_OUT", "ClipRate_R_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_R_OUT},
    {"CLIP_G_OUT", "ClipRate_G_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_G_OUT},
    {"CLIP_B_OUT", "ClipRate_B_out",
     SIXEL_ASSESSMENT_METRIC_CLIP_B_OUT},
    {"CLIP_L_REL", "ClipRate_L_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_L_REL},
    {"CLIP_R_REL", "ClipRate_R_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_R_REL},
    {"CLIP_G_REL", "ClipRate_G_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_G_REL},
    {"CLIP_B_REL", "ClipRate_B_rel",
     SIXEL_ASSESSMENT_METRIC_CLIP_B_REL},
    {"DELTA_CHROMA", "Δ Chroma_mean",
     SIXEL_ASSESSMENT_METRIC_DELTA_CHROMA},
    {"DELTA_E00", "Δ E00_mean", SIXEL_ASSESSMENT_METRIC_DELTA_E00},
    {"GMSD", "GMSD", SIXEL_ASSESSMENT_METRIC_GMSD},
    {"PSNR_Y", "PSNR_Y", SIXEL_ASSESSMENT_METRIC_PSNR_Y},
};

static void
metrics_init(Metrics *metrics)
{
    memset(metrics, 0, sizeof(*metrics));
    metrics->valid_mask = 0u;
}

static void
metrics_set_value(Metrics *metrics,
                  const char *name,
                  double value,
                  int is_valid)
{
    size_t i;
    unsigned char *base;

    base = (unsigned char *)metrics;
    for (i = 0;
            i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]);
            ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            float *field;
            unsigned int mask;

            field = (float *)(void *)(base + g_metric_bindings[i].offset);
            mask = SIXEL_ASSESSMENT_METRIC_MASK(
                    g_metric_bindings[i].metric_id);
            if (is_valid && isfinite(value)) {
                *field = (float)value;
                metrics->valid_mask |= mask;
            } else {
                *field = 0.0f;
                metrics->valid_mask &= ~mask;
            }
            return;
        }
    }
}

#if !(HAVE__STRCASECMP || HAVE__STRCMPI || HAVE_STRCASECMP)
static int
lsqa_ascii_tolower(int ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch + ('a' - 'A');
    }

    return ch;
}


static int
lsqa_ascii_strcasecmp(const char *lhs, const char *rhs)
{
    int result;
    unsigned char lhs_char;
    unsigned char rhs_char;

    result = 0;
    do {
        lhs_char = (unsigned char)*lhs;
        rhs_char = (unsigned char)*rhs;
        result = lsqa_ascii_tolower((int)lhs_char) -
                 lsqa_ascii_tolower((int)rhs_char);
        if (result != 0 || lhs_char == '\0' || rhs_char == '\0') {
            break;
        }
        ++lhs;
        ++rhs;
    } while (1);

    return result;
}
#endif


static int
metric_name_matches(const char *input, const char *target)
{
    if (input == NULL || target == NULL) {
        return 0;
    }
#if HAVE__STRCASECMP
    if (_stricmp(input, target) == 0) {
        return 1;
    }
#elif HAVE__STRCMPI
    if (_strcmpi(input, target) == 0) {
        return 1;
    }
#elif HAVE_STRCASECMP
    if (strcasecmp(input, target) == 0) {
        return 1;
    }
#else
    if (lsqa_ascii_strcasecmp(input, target) == 0) {
        return 1;
    }
#endif
    return 0;
}

static const MetricSpec *
metric_spec_from_name(const char *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0;
            i < sizeof(sixel_metric_specs) / sizeof(sixel_metric_specs[0]);
            ++i) {
        if (metric_name_matches(name, sixel_metric_specs[i].option) ||
                metric_name_matches(name,
                sixel_metric_specs[i].json_key)) {
            return &sixel_metric_specs[i];
        }
    }
    return NULL;
}

static int
metrics_get_value(const Metrics *metrics,
                  const char *name,
                  float *out_value)
{
    size_t i;
    const unsigned char *base;
    unsigned int mask;

    if (metrics == NULL || name == NULL || out_value == NULL) {
        return -1;
    }
    base = (const unsigned char *)metrics;
    mask = 0u;
    for (i = 0; i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]);
            ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            const float *field;

            field = (const float *)(const void *)(base +
                    g_metric_bindings[i].offset);
            mask = SIXEL_ASSESSMENT_METRIC_MASK(
                    g_metric_bindings[i].metric_id);
            if ((metrics->valid_mask & mask) == 0u) {
                return 1;
            }
            *out_value = *field;
            return 0;
        }
    }
    return -1;
}

static unsigned int
metrics_mask_for_name(const char *name)
{
    size_t i;

    if (name == NULL) {
        return 0u;
    }
    for (i = 0; i < sizeof(g_metric_bindings) / sizeof(g_metric_bindings[0]);
            ++i) {
        if (strcmp(name, g_metric_bindings[i].name) == 0) {
            return SIXEL_ASSESSMENT_METRIC_MASK(
                    g_metric_bindings[i].metric_id);
        }
    }
    return 0u;
}

typedef struct MetricItem {
    const char *name;
    float value;
} MetricItem;

static void
verbose_print(const Metrics *m)
{
    MetricItem items[] = {
        {"MS-SSIM", m->ms_ssim},
        {"HighFreqRatio_out", m->high_freq_out},
        {"HighFreqRatio_ref", m->high_freq_ref},
        {"HighFreqRatio_delta", m->high_freq_delta},
        {"StripeScore_ref", m->stripe_ref},
        {"StripeScore_out", m->stripe_out},
        {"StripeScore_rel", m->stripe_rel},
        {"BandingIndex_rel", m->band_run_rel},
        {"BandingIndex_grad_rel", m->band_grad_rel},
        {"ClipRate_L_ref", m->clip_l_ref},
        {"ClipRate_R_ref", m->clip_r_ref},
        {"ClipRate_G_ref", m->clip_g_ref},
        {"ClipRate_B_ref", m->clip_b_ref},
        {"ClipRate_L_out", m->clip_l_out},
        {"ClipRate_R_out", m->clip_r_out},
        {"ClipRate_G_out", m->clip_g_out},
        {"ClipRate_B_out", m->clip_b_out},
        {"ClipRate_L_rel", m->clip_l_rel},
        {"ClipRate_R_rel", m->clip_r_rel},
        {"ClipRate_G_rel", m->clip_g_rel},
        {"ClipRate_B_rel", m->clip_b_rel},
        {"Δ Chroma_mean", m->delta_chroma_mean},
        {"Δ E00_mean", m->delta_e00_mean},
        {"GMSD", m->gmsd_value},
        {"PSNR_Y", m->psnr_y},
    };
    size_t i;

    fprintf(stderr, "\n=== Image Quality Report (Raw Metrics) ===\n");
    for (i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        unsigned int mask;

        mask = metrics_mask_for_name(items[i].name);
        if (mask == 0u || (m->valid_mask & mask) == 0u) {
            fprintf(stderr, "%24s: NaN\n", items[i].name);
        } else {
            fprintf(stderr, "%24s: %.6f\n",
                    items[i].name,
                    items[i].value);
        }
    }
}

typedef struct JsonCollector {
    FILE *stream;
    Metrics *metrics;
} JsonCollector;

static void
json_collect_callback(char const *chunk, size_t length, void *user_data)
{
    JsonCollector *collector;
    char buffer[256];
    char *start;
    char *end;
    char *colon;
    char name[64];
    double value;
    int is_valid;

    collector = (JsonCollector *)user_data;
    if (collector->stream != NULL) {
        if (fwrite(chunk, 1, length, collector->stream) != length) {
            {
                char errbuf[128];

                fprintf(stderr,
                        "Failed to write JSON output: %s\n",
                        lsqa_strerror(errno, errbuf,
                                      sizeof(errbuf)));
            }
        }
    }
    if (collector->metrics == NULL) {
        return;
    }
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1u;
    }
    memcpy(buffer, chunk, length);
    buffer[length] = '\0';
    start = strchr(buffer, '"');
    if (start == NULL) {
        return;
    }
    end = strchr(start + 1, '"');
    if (end == NULL) {
        return;
    }
    colon = strchr(end, ':');
    if (colon == NULL) {
        return;
    }
    colon += 1;
    while (*colon == ' ') {
        colon += 1;
    }
    value = 0.0;
    is_valid = 0;
    if (strncmp(colon, "NaN", 3) == 0) {
        is_valid = 0;
    } else if (strncmp(colon, "null", 4) == 0) {
        is_valid = 0;
    } else {
        char *tail;

        value = strtod(colon, &tail);
        if (tail == colon) {
            return;
        }
        is_valid = 1;
    }
    if ((size_t)(end - start - 1) >= sizeof(name)) {
        return;
    }
    memcpy(name, start + 1, (size_t)(end - start - 1));
    name[end - start - 1] = '\0';
    metrics_set_value(collector->metrics, name, value, is_valid);
}

enum {
    LSQA_COMPARE_COLORSPACE_REFERENCE = (-1)
};

typedef enum LsqaComparePrecision {
    LSQA_COMPARE_PRECISION_REFERENCE = 0,
    LSQA_COMPARE_PRECISION_8BIT = 1,
    LSQA_COMPARE_PRECISION_FLOAT32 = 2
} LsqaComparePrecision;

static int
lsqa_precision_from_pixelformat(int pixelformat)
{
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        return LSQA_COMPARE_PRECISION_FLOAT32;
    }
    return LSQA_COMPARE_PRECISION_8BIT;
}

static int
lsqa_float32_pixelformat_for_colorspace(int colorspace)
{
    switch (colorspace) {
    case SIXEL_COLORSPACE_LINEAR:
        return SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    case SIXEL_COLORSPACE_OKLAB:
        return SIXEL_PIXELFORMAT_OKLABFLOAT32;
    case SIXEL_COLORSPACE_CIELAB:
        return SIXEL_PIXELFORMAT_CIELABFLOAT32;
    case SIXEL_COLORSPACE_DIN99D:
        return SIXEL_PIXELFORMAT_DIN99DFLOAT32;
    default:
        return SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
}

static int
lsqa_target_pixelformat(int colorspace, int precision)
{
    if (precision == LSQA_COMPARE_PRECISION_FLOAT32) {
        return lsqa_float32_pixelformat_for_colorspace(colorspace);
    }
    return SIXEL_PIXELFORMAT_RGB888;
}

static SIXELSTATUS
lsqa_convert_frame_colorspace(sixel_frame_t *frame,
                              int target_colorspace)
{
    SIXELSTATUS status;
    int source_colorspace;
    int pixelformat;
    int width;
    int height;
    int depth;
    size_t size;
    unsigned char *pixels;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    source_colorspace = sixel_frame_get_colorspace(frame);
    if (source_colorspace == target_colorspace) {
        return SIXEL_OK;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    size = (size_t)width * (size_t)height;
    if (size / (size_t)width != (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_INPUT;
    }
    if (size > SIZE_MAX / (size_t)depth) {
        return SIXEL_BAD_INPUT;
    }
    size *= (size_t)depth;

    pixels = sixel_frame_get_pixels(frame);
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        pixels = (unsigned char *)sixel_frame_get_pixels_float32(frame);
    }
    if (pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }

    status = sixel_helper_convert_colorspace(pixels,
                                             size,
                                             pixelformat,
                                             source_colorspace,
                                             target_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_frame_set_colorspace(frame, target_colorspace);
    return SIXEL_OK;
}

static SIXELSTATUS
lsqa_apply_grayscale(sixel_frame_t *frame)
{
    int width;
    int height;
    int pixelformat;
    size_t pixels;
    size_t i;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }
    pixels = (size_t)width * (size_t)height;
    if (pixels / (size_t)width != (size_t)height) {
        return SIXEL_BAD_INPUT;
    }

    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        float *values;

        values = sixel_frame_get_pixels_float32(frame);
        if (values == NULL) {
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i < pixels; ++i) {
            size_t base;
            float y;

            base = i * 3u;
            y = 0.2126f * values[base + 0u]
              + 0.7152f * values[base + 1u]
              + 0.0722f * values[base + 2u];
            values[base + 0u] = y;
            values[base + 1u] = y;
            values[base + 2u] = y;
        }
    } else {
        unsigned char *values;

        values = sixel_frame_get_pixels(frame);
        if (values == NULL) {
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i < pixels; ++i) {
            size_t base;
            unsigned int r;
            unsigned int g;
            unsigned int b;
            unsigned int y;

            base = i * 3u;
            r = (unsigned int)values[base + 0u];
            g = (unsigned int)values[base + 1u];
            b = (unsigned int)values[base + 2u];
            y = (2126u * r + 7152u * g + 722u * b + 5000u) / 10000u;
            if (y > 255u) {
                y = 255u;
            }
            values[base + 0u] = (unsigned char)y;
            values[base + 1u] = (unsigned char)y;
            values[base + 2u] = (unsigned char)y;
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
lsqa_prepare_frame_for_comparison(sixel_frame_t *frame,
                                  int target_colorspace,
                                  int target_precision,
                                  int grayscale_enabled)
{
    SIXELSTATUS status;
    int target_pixelformat;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_strip_alpha(frame, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    target_pixelformat = lsqa_target_pixelformat(target_colorspace,
                                                 target_precision);
    status = sixel_frame_set_pixelformat(frame, target_pixelformat);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = lsqa_convert_frame_colorspace(frame, target_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (grayscale_enabled != 0) {
        status = lsqa_apply_grayscale(frame);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

static int
lsqa_parse_compare_colorspace(char const *argument,
                              int *out_colorspace)
{
    if (argument == NULL || argument[0] == '\0' || out_colorspace == NULL) {
        return -1;
    }

    if (metric_name_matches(argument, "reference")
            || metric_name_matches(argument, "ref")
            || metric_name_matches(argument, "auto")) {
        *out_colorspace = LSQA_COMPARE_COLORSPACE_REFERENCE;
        return 0;
    }
    if (metric_name_matches(argument, "gamma")
            || metric_name_matches(argument, "srgb")) {
        *out_colorspace = SIXEL_COLORSPACE_GAMMA;
        return 0;
    }
    if (metric_name_matches(argument, "linear")) {
        *out_colorspace = SIXEL_COLORSPACE_LINEAR;
        return 0;
    }
    if (metric_name_matches(argument, "oklab")) {
        *out_colorspace = SIXEL_COLORSPACE_OKLAB;
        return 0;
    }
    if (metric_name_matches(argument, "cielab")) {
        *out_colorspace = SIXEL_COLORSPACE_CIELAB;
        return 0;
    }
    if (metric_name_matches(argument, "din99d")) {
        *out_colorspace = SIXEL_COLORSPACE_DIN99D;
        return 0;
    }

    return -1;
}

static int
lsqa_parse_compare_precision(char const *argument,
                             int *out_precision)
{
    if (argument == NULL || argument[0] == '\0' || out_precision == NULL) {
        return -1;
    }

    if (metric_name_matches(argument, "reference")
            || metric_name_matches(argument, "ref")
            || metric_name_matches(argument, "auto")) {
        *out_precision = LSQA_COMPARE_PRECISION_REFERENCE;
        return 0;
    }
    if (metric_name_matches(argument, "8bit")
            || metric_name_matches(argument, "u8")
            || metric_name_matches(argument, "byte")
            || metric_name_matches(argument, "uint8")) {
        *out_precision = LSQA_COMPARE_PRECISION_8BIT;
        return 0;
    }
    if (metric_name_matches(argument, "float32")
            || metric_name_matches(argument, "f32")
            || metric_name_matches(argument, "float")
            || metric_name_matches(argument, "fp32")) {
        *out_precision = LSQA_COMPARE_PRECISION_FLOAT32;
        return 0;
    }

    return -1;
}

static int
lsqa_parse_boolean(char const *argument,
                   int *out_value)
{
    if (argument == NULL || argument[0] == '\0' || out_value == NULL) {
        return -1;
    }

    if (metric_name_matches(argument, "1")
            || metric_name_matches(argument, "true")
            || metric_name_matches(argument, "yes")
            || metric_name_matches(argument, "on")) {
        *out_value = 1;
        return 0;
    }
    if (metric_name_matches(argument, "0")
            || metric_name_matches(argument, "false")
            || metric_name_matches(argument, "no")
            || metric_name_matches(argument, "off")) {
        *out_value = 0;
        return 0;
    }

    return -1;
}

static int
lsqa_copy_option_text(char *buffer,
                      size_t buffer_size,
                      char const *value)
{
    int written;

    if (buffer == NULL || buffer_size == 0u || value == NULL) {
        return -1;
    }
    written = snprintf(buffer, buffer_size, "%s", value);
    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }
    return 0;
}

typedef struct Options {
    const char *ref_path;
    const char *out_path;
    const char *prefix;
    char prefix_buffer[PATH_MAX];
    char loader_order_buffer[PATH_MAX];
    const MetricSpec *metric_spec;
    const char *metric_key;
    const MetricSpec *baseline_spec;
    const char *baseline_key;
    float baseline_value;
    unsigned int metric_mask;
    int metrics_filtered;
    int baseline_enabled;
    int verbose;
    int grayscale_compare;
    int compare_colorspace;
    int compare_precision;
    const char *loader_order;
    int grayscale_specified;
    int compare_colorspace_specified;
    int compare_precision_specified;
    int loader_order_specified;
    char baseline_name[64];
} Options;

static void
lsqa_report_invalid_argument(int short_opt,
                             char const *value,
                             char const *detail);

static void
lsqa_set_parse_error(char const *message);

static int
lsqa_parse_baseline(const char *arg, Options *opts);

static int
lsqa_apply_env_overrides(Options *opts)
{
    char *metrics_env;
    char *baseline_env;
    char *grayscale_env;
    char *colorspace_env;
    char *precision_env;
    char *loaders_env;
    const MetricSpec *metric_spec;
    int boolean_value;
    int status;

    if (opts == NULL) {
        return -1;
    }

    metrics_env = NULL;
    baseline_env = NULL;
    grayscale_env = NULL;
    colorspace_env = NULL;
    precision_env = NULL;
    loaders_env = NULL;
    metric_spec = NULL;
    boolean_value = 0;
    status = 0;

    metrics_env = lsqa_getenv_dup("LSQA_METRICS");
    baseline_env = lsqa_getenv_dup("LSQA_BASELINE");
    grayscale_env = lsqa_getenv_dup("LSQA_GRAYSCALE");
    colorspace_env = lsqa_getenv_dup("LSQA_COMPARE_COLORSPACE");
    precision_env = lsqa_getenv_dup("LSQA_COMPARE_PRECISION");
    loaders_env = lsqa_getenv_dup("LSQA_LOADERS");

    if (metrics_env != NULL && metrics_env[0] != '\0'
            && opts->metric_spec == NULL) {
        metric_spec = metric_spec_from_name(metrics_env);
        if (metric_spec == NULL) {
            lsqa_report_invalid_argument('m',
                                         metrics_env,
                                         "Unknown metric name.");
            status = -1;
            goto cleanup;
        }
        opts->metric_spec = metric_spec;
        opts->metric_key = metric_spec->json_key;
        opts->metric_mask = SIXEL_ASSESSMENT_METRIC_MASK(
            metric_spec->metric_id);
        opts->metrics_filtered = 1;
        if (opts->baseline_spec != NULL
                && opts->baseline_spec != opts->metric_spec) {
            lsqa_set_parse_error("lsqa: baseline metric must match -m.");
            status = -1;
            goto cleanup;
        }
    }

    if (baseline_env != NULL && baseline_env[0] != '\0'
            && !opts->baseline_enabled) {
        if (lsqa_parse_baseline(baseline_env, opts) != 0) {
            status = -1;
            goto cleanup;
        }
        if (opts->metric_spec != NULL
                && opts->metric_spec != opts->baseline_spec) {
            lsqa_set_parse_error("lsqa: baseline metric must match -m.");
            status = -1;
            goto cleanup;
        }
    }

    if (grayscale_env != NULL && grayscale_env[0] != '\0'
            && !opts->grayscale_specified) {
        if (lsqa_parse_boolean(grayscale_env, &boolean_value) != 0) {
            lsqa_report_invalid_argument('g',
                                         grayscale_env,
                                         "Expected 0/1 or false/true.");
            status = -1;
            goto cleanup;
        }
        opts->grayscale_compare = boolean_value;
    }

    if (colorspace_env != NULL && colorspace_env[0] != '\0'
            && !opts->compare_colorspace_specified) {
        if (lsqa_parse_compare_colorspace(colorspace_env,
                                          &opts->compare_colorspace) != 0) {
            lsqa_report_invalid_argument(
                'W',
                colorspace_env,
                "Expected reference/gamma/linear/oklab/cielab/din99d.");
            status = -1;
            goto cleanup;
        }
    }

    if (precision_env != NULL && precision_env[0] != '\0'
            && !opts->compare_precision_specified) {
        if (lsqa_parse_compare_precision(precision_env,
                                         &opts->compare_precision) != 0) {
            lsqa_report_invalid_argument(
                'P',
                precision_env,
                "Expected reference/8bit/float32.");
            status = -1;
            goto cleanup;
        }
    }

    if (loaders_env != NULL && loaders_env[0] != '\0'
            && !opts->loader_order_specified) {
        if (lsqa_copy_option_text(opts->loader_order_buffer,
                                  sizeof(opts->loader_order_buffer),
                                  loaders_env) != 0) {
            lsqa_report_invalid_argument('L',
                                         loaders_env,
                                         "Loader list is too long.");
            status = -1;
            goto cleanup;
        }
        opts->loader_order = opts->loader_order_buffer;
    }

cleanup:
    free(loaders_env);
    free(precision_env);
    free(colorspace_env);
    free(grayscale_env);
    free(baseline_env);
    free(metrics_env);
    return status;
}

/*
 * Per-option help text for --help output and contextual errors.
 * The layout mirrors converter CLI tables to keep behavior consistent.
 */
static cli_option_help_t const g_option_help_table[] = {
    {
        'm',
        "metrics",
        "-m NAME, --metrics=NAME    limit computation to NAME and print\n"
        "                           only that value.\n"
    },
    {
        'b',
        "baseline",
        "-b METRIC:VALUE, --baseline=METRIC:VALUE\n"
        "                           fail when METRIC is below VALUE.\n"
    },
    {
        '%',
        "env",
        "-% KEY=VALUE, --env=KEY=VALUE\n"
        "                           set process environment variable\n"
        "                           before assessment. Repeatable.\n"
    },
    {
        'g',
        "grayscale",
        "-g, --grayscale            compare using grayscale luminance.\n"
    },
    {
        'W',
        "compare-colorspace",
        "-W COLORSPACE, --compare-colorspace=COLORSPACE\n"
        "                           comparison colorspace (reference,\n"
        "                           gamma, linear, oklab,\n"
        "                           cielab, din99d).\n"
    },
    {
        'P',
        "compare-precision",
        "-P PRECISION, --compare-precision=PRECISION\n"
        "                           comparison precision (reference,\n"
        "                           8bit, float32).\n"
    },
    {
        'L',
        "loaders",
        "-L LIST, --loaders=LIST    override loader order (same syntax\n"
        "                           as img2sixel -L).\n"
    },
    {
        'H',
        "help",
        "-H, --help                 show this help.\n"
    }
};

static char const g_option_help_fallback[] =
    "    Refer to \"lsqa -H\" for more details.\n";

static size_t
lsqa_option_help_count(void)
{
    return sizeof(g_option_help_table) /
        sizeof(g_option_help_table[0]);
}

static char const g_lsqa_optstring[] = "m:b:%:gW:P:L:Hh";

static void
lsqa_print_option_help(FILE *stream)
{
    cli_print_option_help(stream,
                          g_option_help_table,
                          lsqa_option_help_count());
}

static void
lsqa_report_missing_argument(int short_opt)
{
    cli_report_missing_argument("lsqa",
                                g_option_help_fallback,
                                g_option_help_table,
                                lsqa_option_help_count(),
                                short_opt);
}

static void
lsqa_report_missing_argument_callback(int short_opt, void *user_data)
{
    (void)user_data;

    lsqa_report_missing_argument(short_opt);
}

static void
lsqa_report_unrecognized_option(int short_opt, char const *token)
{
    cli_report_unrecognized_option("lsqa", short_opt, token);
}

static void
lsqa_handle_getopt_error(int short_opt, char const *token)
{
    cli_option_help_t const *entry;
    cli_option_help_t const *long_entry;
    char const *long_name;

    entry = NULL;
    long_entry = NULL;
    long_name = NULL;

    if (short_opt > 0) {
        entry = cli_find_option_help(g_option_help_table,
                                     lsqa_option_help_count(),
                                     short_opt);
        if (entry != NULL) {
            lsqa_report_missing_argument(short_opt);
            return;
        }
    }

    if (token != NULL && token[0] != '\0') {
        if (strncmp(token, "--", 2) == 0) {
            long_name = token + 2;
        } else if (token[0] == '-') {
            long_name = token + 1;
        }
        if (long_name != NULL && long_name[0] != '\0') {
            long_entry = cli_find_option_help_by_long_name(
                g_option_help_table,
                lsqa_option_help_count(),
                long_name);
            if (long_entry != NULL) {
                lsqa_report_missing_argument(long_entry->short_opt);
                return;
            }
        }
    }

    lsqa_report_unrecognized_option(short_opt, token);
}

static int
lsqa_guard_missing_argument(int short_opt, char *const *argv)
{
    return cli_guard_missing_argument(short_opt,
                                      argv,
                                      optarg,
                                      &optind,
                                      g_lsqa_optstring,
                                      g_option_help_table,
                                      lsqa_option_help_count(),
                                      NULL,
                                      NULL,
                                      lsqa_report_missing_argument_callback,
                                      NULL);
}

static void
lsqa_report_invalid_argument(int short_opt,
                             char const *value,
                             char const *detail)
{
    char buffer[1024];
    char detail_copy[1024];
    cli_option_help_t const *entry;
    char const *long_opt;
    char const *help_text;
    char const *argument;
    size_t offset;
    int written;

    memset(buffer, 0, sizeof(buffer));
    memset(detail_copy, 0, sizeof(detail_copy));
    entry = cli_find_option_help(g_option_help_table,
                                 lsqa_option_help_count(),
                                 short_opt);
    long_opt = (entry != NULL && entry->long_opt != NULL)
        ? entry->long_opt : "?";
    help_text = (entry != NULL && entry->help != NULL)
        ? entry->help : g_option_help_fallback;
    argument = (value != NULL && value[0] != '\0')
        ? value : "(missing)";
    offset = 0u;

    written = snprintf(buffer,
                       sizeof(buffer),
                       "\\fW'%s'\\fP is invalid argument for "
                       "\\fB-%c\\fP,\\fB--%s\\fP option:\n\n",
                       argument,
                       (char)short_opt,
                       long_opt);
    if (written < 0) {
        written = 0;
    }
    if ((size_t)written >= sizeof(buffer)) {
        offset = sizeof(buffer) - 1u;
    } else {
        offset = (size_t)written;
    }

    if (detail != NULL && detail[0] != '\0' && offset < sizeof(buffer) - 1u) {
        (void) snprintf(detail_copy,
                        sizeof(detail_copy),
                        "%s\n",
                        detail);
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           detail_copy);
        if (written < 0) {
            written = 0;
        }
        if ((size_t)written >= sizeof(buffer) - offset) {
            offset = sizeof(buffer) - 1u;
        } else {
            offset += (size_t)written;
        }
    }

    if (offset < sizeof(buffer) - 1u) {
        written = snprintf(buffer + offset,
                           sizeof(buffer) - offset,
                           "%s",
                           help_text);
        if (written < 0) {
            written = 0;
        }
    }

    sixel_helper_set_additional_message(buffer);
}

static void
print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-m NAME] [-b METRIC:VALUE] [-g]\n"
            "             [-W COLORSPACE] [-P PRECISION] [-L LIST]\n"
            "             <reference> [output]\n",
            prog);
    fprintf(stderr,
            "       %s [-m NAME] [-b METRIC:VALUE] [-g]\n"
            "             [-W COLORSPACE] [-P PRECISION] [-L LIST]\n"
            "             <reference> < output\n",
            prog);
    fprintf(stderr,
            "       %s [-m NAME] [-b METRIC:VALUE] [-g]\n"
            "             [-W COLORSPACE] [-P PRECISION] [-L LIST]\n"
            "             <reference> - < output\n",
            prog);
    fprintf(stderr,
    "  -m, --metrics NAME  limit computation to a single metric\n");
    fprintf(stderr,
            "                        and print only that value\n");
    fprintf(stderr,
            "  -b, --baseline METRIC:VALUE\n");
    fprintf(stderr,
            "                        exit nonzero if METRIC is below VALUE\n");
    fprintf(stderr,
            "  -g, --grayscale     compare in grayscale\n");
    fprintf(stderr,
            "  -W, --compare-colorspace COLORSPACE\n");
    fprintf(stderr,
            "                        reference/gamma/linear/oklab/\n"
            "                        cielab/din99d\n");
    fprintf(stderr,
            "  -P, --compare-precision PRECISION\n");
    fprintf(stderr,
            "                        reference/8bit/float32\n");
    fprintf(stderr,
            "  -L, --loaders LIST  loader order (img2sixel -L syntax)\n");
}

static void
show_help(void)
{
    /*
     * Help text must go to stdout to match converter tools and allow piping.
     */
    fprintf(stdout,
            "Usage: lsqa [-m NAME] [-b METRIC:VALUE] [-g]\n"
            "            [-W COLORSPACE] [-P PRECISION] [-L LIST]\n"
            "            <reference> [output]\n"
            "       lsqa [-m NAME] [-b METRIC:VALUE] [-g]\n"
            "            [-W COLORSPACE] [-P PRECISION] [-L LIST]\n"
            "            <reference> < output\n"
            "       lsqa [-m NAME] [-b METRIC:VALUE] [-g]\n"
            "            [-W COLORSPACE] [-P PRECISION] [-L LIST]\n"
            "            <reference> - < output\n"
            "\n"
            "Options:\n");
    lsqa_print_option_help(stdout);
    fprintf(stdout,
            "\n"
            "Environment overrides:\n"
            "  LSQA_METRICS=NAME\n"
            "  LSQA_BASELINE=METRIC:VALUE\n"
            "  LSQA_GRAYSCALE=0|1\n"
            "  LSQA_COMPARE_COLORSPACE=reference|gamma|linear|oklab|cielab|din99d\n"
            "  LSQA_COMPARE_PRECISION=reference|8bit|float32\n"
            "  LSQA_LOADERS=LIST\n"
            "  LSQA_PREFIX=NAME\n"
            "  LSQA_VERBOSE=0|1\n"
            "\n"
            "Exit codes:\n"
            "  0  success\n"
            "  2  invalid arguments\n"
            "  3  failed to load input images\n"
            "  4  assessment or output failure\n"
            "  5  baseline metric below threshold\n");
}

static void
lsqa_set_parse_error(char const *message)
{
    if (message == NULL || message[0] == '\0') {
        return;
    }
    sixel_helper_set_additional_message(message);
}

static int
lsqa_parse_baseline(const char *arg, Options *opts)
{
    const char *colon;
    size_t name_len;
    const MetricSpec *spec;
    const char *value_text;
    char *tail;
    double value;

    if (arg == NULL || arg[0] == '\0') {
        lsqa_report_invalid_argument('b', arg,
                                     "Missing METRIC:VALUE baseline.");
        return -1;
    }

    colon = strchr(arg, ':');
    if (colon == NULL) {
        lsqa_report_invalid_argument('b', arg,
                                     "Expected METRIC:VALUE format.");
        return -1;
    }

    name_len = (size_t)(colon - arg);
    if (name_len == 0u) {
        lsqa_report_invalid_argument('b', arg,
                                     "Metric name is empty.");
        return -1;
    }
    if (name_len >= sizeof(opts->baseline_name)) {
        lsqa_report_invalid_argument('b', arg,
                                     "Metric name is too long.");
        return -1;
    }

    memcpy(opts->baseline_name, arg, name_len);
    opts->baseline_name[name_len] = '\0';
    spec = metric_spec_from_name(opts->baseline_name);
    if (spec == NULL) {
        lsqa_report_invalid_argument('b', arg,
                                     "Unknown metric name.");
        return -1;
    }

    value_text = colon + 1;
    if (value_text[0] == '\0') {
        lsqa_report_invalid_argument('b', arg,
                                     "Baseline value is empty.");
        return -1;
    }

    value = strtod(value_text, &tail);
    if (tail == value_text) {
        lsqa_report_invalid_argument('b', arg,
                                     "Baseline value is not a number.");
        return -1;
    }
    while (*tail == ' ' || *tail == '\t') {
        tail += 1;
    }
    if (*tail != '\0') {
        lsqa_report_invalid_argument('b', arg,
                                     "Unexpected characters in value.");
        return -1;
    }
    if (!isfinite(value)) {
        lsqa_report_invalid_argument('b', arg,
                                     "Baseline value is not finite.");
        return -1;
    }

    opts->baseline_spec = spec;
    opts->baseline_key = spec->json_key;
    opts->baseline_value = (float)value;
    opts->baseline_enabled = 1;
    return 0;
}

static int
lsqa_check_baseline(const Options *opts,
                    const Metrics *metrics,
                    float *out_value)
{
    int metric_status;
    float metric_value;

    if (opts == NULL || metrics == NULL || out_value == NULL) {
        return -1;
    }
    if (!opts->baseline_enabled) {
        return 0;
    }

    metric_value = 0.0f;
    metric_status = metrics_get_value(metrics,
                                      opts->baseline_key,
                                      &metric_value);
    if (metric_status < 0) {
        return -1;
    }
    if (metric_status > 0) {
        return 2;
    }
    *out_value = metric_value;
    if (metric_value < opts->baseline_value) {
        return 1;
    }
    return 0;
}

static int
lsqa_build_reordered_argv(int argc,
                          char **argv,
                          char ***out_argv,
                          int *out_argc)
{
    char **reordered;
    char **options;
    char **positionals;
    int opt_count;
    int pos_count;
    int end_of_options;
    int i;
    int short_opt;
    int needs_arg;
    int known;
    int new_argc;
    char *token;
    char *next_token;
    char *equals;
    size_t token_len;

    reordered = NULL;
    options = NULL;
    positionals = NULL;
    opt_count = 0;
    pos_count = 0;
    end_of_options = 0;
    new_argc = 0;

    if (out_argv != NULL) {
        *out_argv = NULL;
    }
    if (out_argc != NULL) {
        *out_argc = 0;
    }

    if (argc <= 0 || argv == NULL) {
        return -1;
    }

    reordered = (char **)calloc((size_t)argc + 1u, sizeof(char *));
    options = (char **)calloc((size_t)argc, sizeof(char *));
    positionals = (char **)calloc((size_t)argc, sizeof(char *));
    if (reordered == NULL || options == NULL || positionals == NULL) {
        free(reordered);
        free(options);
        free(positionals);
        lsqa_set_parse_error("lsqa: out of memory while parsing args.");
        return -1;
    }

    for (i = 1; i < argc; ++i) {
        token = argv[i];
        if (token == NULL) {
            continue;
        }
        if (end_of_options != 0) {
            positionals[pos_count++] = token;
            continue;
        }
        if (strcmp(token, "--") == 0) {
            end_of_options = 1;
            continue;
        }
        if (token[0] == '-' && token[1] != '\0') {
            short_opt = 0;
            known = cli_token_is_known_option(g_option_help_table,
                                              lsqa_option_help_count(),
                                              token,
                                              &short_opt);
            if (known == 0) {
                lsqa_report_unrecognized_option(0, token);
                goto error;
            }
            needs_arg = cli_option_requires_argument(g_lsqa_optstring,
                                                     short_opt);
            if (needs_arg != 0) {
                token_len = strlen(token);
                equals = strchr(token, '=');
                if (token[1] == (char)short_opt && token_len > 2u) {
                    options[opt_count++] = token;
                    continue;
                }
                if (equals != NULL) {
                    options[opt_count++] = token;
                    continue;
                }
                if (i + 1 >= argc) {
                    lsqa_report_missing_argument(short_opt);
                    goto error;
                }
                next_token = argv[i + 1];
                if (next_token == NULL) {
                    lsqa_report_missing_argument(short_opt);
                    goto error;
                }
                if (next_token[0] == '-' && next_token[1] != '\0') {
                    known = cli_token_is_known_option(
                        g_option_help_table,
                        lsqa_option_help_count(),
                        next_token,
                        NULL);
                    if (known != 0) {
                        lsqa_report_missing_argument(short_opt);
                        goto error;
                    }
                }
                options[opt_count++] = token;
                options[opt_count++] = next_token;
                i += 1;
                continue;
            }
            options[opt_count++] = token;
            continue;
        }
        positionals[pos_count++] = token;
    }

    new_argc = 1 + opt_count + pos_count;
    reordered[0] = argv[0];
    for (i = 0; i < opt_count; ++i) {
        reordered[1 + i] = options[i];
    }
    for (i = 0; i < pos_count; ++i) {
        reordered[1 + opt_count + i] = positionals[i];
    }
    reordered[new_argc] = NULL;

    if (out_argv != NULL) {
        *out_argv = reordered;
    }
    if (out_argc != NULL) {
        *out_argc = new_argc;
    }

    free(options);
    free(positionals);
    return 0;

error:
    free(reordered);
    free(options);
    free(positionals);
    return -1;
}

static int
parse_args(int argc, char **argv, Options *opts)
{
    const char *ref_arg;
    const char *out_arg;
    const char *base_start;
    const char *dot_pos;
    const char *slash_pos;
#if defined(_WIN32)
    const char *alt_slash_pos;
#endif
    char *verbose_env;
    char *prefix_env;
    const char *metrics_arg;
    const char *baseline_arg;
    const MetricSpec *metric_spec;
    size_t prefix_len;
    int status;
    int argi;
    int parse_status;
    char **scan_argv;
    int scan_argc;
    int opt;
    int verbose_value;
    char detail_buffer[256];
#if HAVE_GETOPT_LONG
    int long_opt;
    int option_index;
#endif

    opts->ref_path = NULL;
    opts->out_path = "-";
    opts->prefix = "report";
    opts->metric_spec = NULL;
    opts->metric_key = NULL;
    opts->baseline_spec = NULL;
    opts->baseline_key = NULL;
    opts->baseline_value = 0.0f;
    opts->metric_mask = SIXEL_ASSESSMENT_METRIC_MASK_ALL;
    opts->metrics_filtered = 0;
    opts->baseline_enabled = 0;
    opts->verbose = 0;
    opts->grayscale_compare = 0;
    opts->compare_colorspace = LSQA_COMPARE_COLORSPACE_REFERENCE;
    opts->compare_precision = LSQA_COMPARE_PRECISION_REFERENCE;
    opts->loader_order = NULL;
    opts->grayscale_specified = 0;
    opts->compare_colorspace_specified = 0;
    opts->compare_precision_specified = 0;
    opts->loader_order_specified = 0;
    opts->prefix_buffer[0] = '\0';
    opts->loader_order_buffer[0] = '\0';
    opts->baseline_name[0] = '\0';

    argi = 1;
    parse_status = 0;
    scan_argv = NULL;
    scan_argc = 0;
    opt = 0;
    verbose_value = 0;
    verbose_env = NULL;
    prefix_env = NULL;
#if HAVE_GETOPT_LONG
    long_opt = 0;
    option_index = 0;
#endif

    if (lsqa_build_reordered_argv(argc, argv,
                                  &scan_argv,
                                  &scan_argc) != 0) {
        parse_status = -1;
        goto cleanup;
    }

    optind = 1;

    for (;;) {
#if HAVE_GETOPT_LONG
        struct option long_options[] = {
            {"metrics", required_argument, &long_opt, 'm'},
            {"baseline", required_argument, &long_opt, 'b'},
            {"env", required_argument, &long_opt, '%'},
            {"grayscale", no_argument, &long_opt, 'g'},
            {"compare-colorspace", required_argument, &long_opt, 'W'},
            {"compare-precision", required_argument, &long_opt, 'P'},
            {"loaders", required_argument, &long_opt, 'L'},
            {"help", no_argument, &long_opt, 'H'},
            {0, 0, 0, 0}
        };

        opt = getopt_long(scan_argc, scan_argv, g_lsqa_optstring,
                          long_options, &option_index);
#else
        opt = getopt(scan_argc, scan_argv, g_lsqa_optstring);
#endif
        if (opt == -1) {
            break;
        }
#if HAVE_GETOPT_LONG
        if (opt == 0) {
            opt = long_opt;
        }
#endif

        if (opt > 0) {
            if (lsqa_guard_missing_argument(opt, argv) != 0) {
                parse_status = -1;
                goto cleanup;
            }
        }

        switch (opt) {
        case 'H':
        case 'h':
            parse_status = 1;
            goto cleanup;
        case '%':
            if (cli_apply_env_assignment(optarg,
                                         detail_buffer,
                                         sizeof(detail_buffer)) != 0) {
                lsqa_set_parse_error(detail_buffer);
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'g':
            opts->grayscale_compare = 1;
            opts->grayscale_specified = 1;
            break;
        case 'W':
            if (lsqa_parse_compare_colorspace(
                        optarg,
                        &opts->compare_colorspace) != 0) {
                lsqa_report_invalid_argument(
                    'W',
                    optarg,
                    "Expected reference/gamma/linear/oklab/cielab/din99d.");
                parse_status = -1;
                goto cleanup;
            }
            opts->compare_colorspace_specified = 1;
            break;
        case 'P':
            if (lsqa_parse_compare_precision(
                        optarg,
                        &opts->compare_precision) != 0) {
                lsqa_report_invalid_argument(
                    'P',
                    optarg,
                    "Expected reference/8bit/float32.");
                parse_status = -1;
                goto cleanup;
            }
            opts->compare_precision_specified = 1;
            break;
        case 'L':
            if (opts->loader_order != NULL) {
                lsqa_set_parse_error(
                    "lsqa: loader order already specified.");
                parse_status = -1;
                goto cleanup;
            }
            opts->loader_order = optarg;
            opts->loader_order_specified = 1;
            break;
        case 'b':
            if (opts->baseline_enabled) {
                lsqa_set_parse_error(
                    "lsqa: baseline already specified.");
                parse_status = -1;
                goto cleanup;
            }
            baseline_arg = optarg;
            if (lsqa_parse_baseline(baseline_arg, opts) != 0) {
                parse_status = -1;
                goto cleanup;
            }
            if (opts->metric_spec != NULL &&
                    opts->metric_spec != opts->baseline_spec) {
                lsqa_set_parse_error(
                    "lsqa: baseline metric must match -m.");
                parse_status = -1;
                goto cleanup;
            }
            break;
        case 'm':
            metrics_arg = optarg;
            metric_spec = metric_spec_from_name(metrics_arg);
            if (metric_spec == NULL) {
                lsqa_report_invalid_argument('m',
                                             metrics_arg,
                                             "Unknown metric name.");
                parse_status = -1;
                goto cleanup;
            }
            if (opts->metric_spec != NULL) {
                lsqa_set_parse_error(
                    "lsqa: metric already specified.");
                parse_status = -1;
                goto cleanup;
            }
            opts->metric_spec = metric_spec;
            opts->metric_key = metric_spec->json_key;
            opts->metric_mask = SIXEL_ASSESSMENT_METRIC_MASK(
                metric_spec->metric_id);
            opts->metrics_filtered = 1;
            if (opts->baseline_spec != NULL &&
                    opts->baseline_spec != opts->metric_spec) {
                lsqa_set_parse_error(
                    "lsqa: baseline metric must match -m.");
                parse_status = -1;
                goto cleanup;
            }
            break;
        case '?':
            lsqa_handle_getopt_error(optopt,
                                     (optind > 0 && optind <= scan_argc)
                                         ? scan_argv[optind - 1]
                                         : NULL);
            parse_status = -1;
            goto cleanup;
        default:
            lsqa_report_unrecognized_option(opt, NULL);
            parse_status = -1;
            goto cleanup;
        }
    }

    argi = optind;
    if (scan_argc - argi < 1 || scan_argc - argi > 2) {
        lsqa_set_parse_error("lsqa: invalid number of arguments.");
        parse_status = -1;
        goto cleanup;
    }

    ref_arg = scan_argv[argi];
    opts->ref_path = ref_arg;
    argi += 1;

    if (scan_argc - argi >= 1) {
        out_arg = scan_argv[argi];
        if (strcmp(out_arg, "-") == 0) {
            opts->out_path = "-";
        } else {
            opts->out_path = out_arg;
        }
    }

    if (lsqa_apply_env_overrides(opts) != 0) {
        parse_status = -1;
        goto cleanup;
    }

    base_start = opts->out_path;
    if (strcmp(opts->out_path, "-") == 0) {
        base_start = ref_arg;
    }

    slash_pos = strrchr(base_start, '/');
#if defined(_WIN32)
    alt_slash_pos = strrchr(base_start, '\\');
    if (alt_slash_pos != NULL) {
        if (slash_pos == NULL || alt_slash_pos > slash_pos) {
            slash_pos = alt_slash_pos;
        }
    }
#endif
    if (slash_pos != NULL) {
        base_start = slash_pos + 1;
    }
    dot_pos = strrchr(base_start, '.');
    if (dot_pos != NULL && dot_pos > base_start) {
        prefix_len = (size_t)(dot_pos - base_start);
    } else {
        prefix_len = strlen(base_start);
    }
    if (prefix_len >= sizeof(opts->prefix_buffer)) {
        prefix_len = sizeof(opts->prefix_buffer) - 1u;
    }
    if (prefix_len == 0) {
        status = snprintf(opts->prefix_buffer,
                          sizeof(opts->prefix_buffer),
                          "report");
        if (status < 0) {
            parse_status = -1;
            goto cleanup;
        }
    } else {
        memcpy(opts->prefix_buffer, base_start, prefix_len);
        opts->prefix_buffer[prefix_len] = '\0';
    }
    opts->prefix = opts->prefix_buffer;

    prefix_env = lsqa_getenv_dup("LSQA_PREFIX");
    if (prefix_env != NULL && prefix_env[0] != '\0') {
        status = snprintf(opts->prefix_buffer,
                          sizeof(opts->prefix_buffer),
                          "%s",
                          prefix_env);
        if (status < 0) {
            parse_status = -1;
            goto cleanup;
        }
        opts->prefix = opts->prefix_buffer;
    }

    verbose_env = lsqa_getenv_dup("LSQA_VERBOSE");
    if (verbose_env != NULL && verbose_env[0] != '\0') {
        if (lsqa_parse_boolean(verbose_env, &verbose_value) != 0) {
            lsqa_set_parse_error(
                "lsqa: LSQA_VERBOSE expects 0/1 or false/true.");
            parse_status = -1;
            goto cleanup;
        }
        opts->verbose = verbose_value;
    }

    parse_status = 0;

cleanup:
    free(prefix_env);
    free(verbose_env);
    free(scan_argv);
    return parse_status;
}

int
main(int argc, char **argv)
{
    Options opts;
    sixel_allocator_t *allocator;
    sixel_assessment_t *assessment;
    SIXELSTATUS status;
    sixel_frame_t *ref_frame;
    sixel_frame_t *out_frame;
    Metrics metrics;
    JsonCollector collector;
    size_t path_len;
    char *json_path;
    FILE *fp;
    int exit_code;
    int metric_status;
    float metric_value;
    int baseline_status;
    float baseline_value;
    char const *parse_message;
    int parse_status;
    int reference_colorspace;
    int reference_precision;
    int target_colorspace;
    int target_precision;

    allocator = NULL;
    assessment = NULL;
    ref_frame = NULL;
    out_frame = NULL;
    fp = NULL;
    json_path = NULL;
    exit_code = LSQA_EXIT_RUNTIME_FAILED;
    metric_status = 0;
    metric_value = 0.0f;
    baseline_status = 0;
    baseline_value = 0.0f;
    parse_message = NULL;
    parse_status = 0;
    reference_colorspace = SIXEL_COLORSPACE_GAMMA;
    reference_precision = LSQA_COMPARE_PRECISION_8BIT;
    target_colorspace = SIXEL_COLORSPACE_GAMMA;
    target_precision = LSQA_COMPARE_PRECISION_8BIT;

    parse_status = parse_args(argc, argv, &opts);
    if (parse_status != 0) {
        if (parse_status > 0) {
            show_help();
            return LSQA_EXIT_SUCCESS;
        }
        parse_message = sixel_helper_get_additional_message();
        if (parse_message != NULL && parse_message[0] != '\0') {
            fprintf(stderr, "%s\n", parse_message);
        }
        print_usage(argv[0]);
        return LSQA_EXIT_BAD_ARGS;
    }

    status = sixel_allocator_new(&allocator,
                                 malloc,
                                 calloc,
                                 realloc,
                                 free);
    if (SIXEL_FAILED(status) || allocator == NULL) {
        fprintf(stderr,
                "Failed to allocate loader: %s\n",
                sixel_helper_format_error(status));
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    if (load_frame(opts.ref_path,
                   allocator,
                   opts.loader_order,
                   &ref_frame) != 0) {
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_LOAD_FAILED;
    }
    if (load_frame(opts.out_path,
                   allocator,
                   opts.loader_order,
                   &out_frame) != 0) {
        sixel_frame_unref(ref_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_LOAD_FAILED;
    }

    reference_colorspace = sixel_frame_get_colorspace(ref_frame);
    reference_precision = lsqa_precision_from_pixelformat(
        sixel_frame_get_pixelformat(ref_frame));
    if (opts.compare_colorspace == LSQA_COMPARE_COLORSPACE_REFERENCE) {
        target_colorspace = reference_colorspace;
    } else {
        target_colorspace = opts.compare_colorspace;
    }
    if (opts.compare_precision == LSQA_COMPARE_PRECISION_REFERENCE) {
        target_precision = reference_precision;
    } else {
        target_precision = opts.compare_precision;
    }

    status = lsqa_prepare_frame_for_comparison(ref_frame,
                                               target_colorspace,
                                               target_precision,
                                               opts.grayscale_compare);
    if (SIXEL_FAILED(status)) {
        char const *detail;

        detail = sixel_helper_get_additional_message();
        if (detail != NULL && detail[0] != '\0') {
            fprintf(stderr,
                    "Failed to normalize reference frame: %s (%s)\n",
                    sixel_helper_format_error(status),
                    detail);
        } else {
            fprintf(stderr,
                    "Failed to normalize reference frame: %s\n",
                    sixel_helper_format_error(status));
        }
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }
    status = lsqa_prepare_frame_for_comparison(out_frame,
                                               target_colorspace,
                                               target_precision,
                                               opts.grayscale_compare);
    if (SIXEL_FAILED(status)) {
        char const *detail;

        detail = sixel_helper_get_additional_message();
        if (detail != NULL && detail[0] != '\0') {
            fprintf(stderr,
                    "Failed to normalize output frame: %s (%s)\n",
                    sixel_helper_format_error(status),
                    detail);
        } else {
            fprintf(stderr,
                    "Failed to normalize output frame: %s\n",
                    sixel_helper_format_error(status));
        }
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    status = sixel_assessment_new(&assessment, allocator);
    if (SIXEL_FAILED(status) || assessment == NULL) {
        fprintf(stderr,
                "Failed to create assessment object: %s\n",
                sixel_helper_format_error(status));
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    sixel_assessment_select_sections(assessment,
                                     SIXEL_ASSESSMENT_SECTION_QUALITY);

    sixel_assessment_select_metrics(assessment, opts.metric_mask);

    status = sixel_assessment_analyze(assessment, ref_frame, out_frame);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Assessment failed: %s\n",
                sixel_helper_format_error(status));
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    metrics_init(&metrics);
    collector.metrics = &metrics;

    if (opts.metrics_filtered || opts.baseline_enabled) {
        collector.stream = NULL;
        status = sixel_assessment_get_json(assessment,
                                           SIXEL_ASSESSMENT_SECTION_QUALITY,
                                           json_collect_callback,
                                           &collector);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr,
                    "Failed to emit JSON: %s\n",
                    sixel_helper_format_error(status));
            sixel_assessment_unref(assessment);
            sixel_frame_unref(ref_frame);
            sixel_frame_unref(out_frame);
            sixel_allocator_unref(allocator);
            return LSQA_EXIT_RUNTIME_FAILED;
        }

        if (opts.metrics_filtered) {
            metric_status = metrics_get_value(&metrics,
                                              opts.metric_key,
                                              &metric_value);
            if (metric_status < 0) {
                fprintf(stderr,
                        "Requested metric not found in output: %s\n",
                        opts.metric_key);
                sixel_assessment_unref(assessment);
                sixel_frame_unref(ref_frame);
                sixel_frame_unref(out_frame);
                sixel_allocator_unref(allocator);
                return LSQA_EXIT_RUNTIME_FAILED;
            }
            if (metric_status > 0) {
                fprintf(stdout, "nan\n");
            } else {
                fprintf(stdout, "%.6f\n", metric_value);
            }
        }

        if (opts.verbose) {
            verbose_print(&metrics);
        }

        baseline_status = lsqa_check_baseline(&opts,
                                              &metrics,
                                              &baseline_value);
        if (baseline_status != 0) {
            if (baseline_status < 0) {
                fprintf(stderr,
                        "Metric %s is unavailable\n",
                        opts.baseline_key);
                exit_code = LSQA_EXIT_RUNTIME_FAILED;
            } else if (baseline_status == 2) {
                fprintf(stderr,
                        "Metric %s is NaN\n",
                        opts.baseline_key);
                exit_code = LSQA_EXIT_BASELINE_FAILED;
            } else {
                fprintf(stderr,
                        "%s %.6f is below baseline %.6f\n",
                        opts.baseline_key,
                        baseline_value,
                        opts.baseline_value);
                exit_code = LSQA_EXIT_BASELINE_FAILED;
            }
        } else {
            exit_code = LSQA_EXIT_SUCCESS;
        }
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return exit_code;
    }

    path_len = strlen(opts.prefix) + strlen("_metrics.json") + 1u;
    json_path = (char *)malloc(path_len);
    if (json_path == NULL) {
        fprintf(stderr, "Out of memory while building JSON path\n");
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }
    snprintf(json_path, path_len, "%s_metrics.json", opts.prefix);

    fp = lsqa_fopen_write(json_path);
    if (fp == NULL) {
        char errbuf[128];

        fprintf(stderr,
                "Failed to open %s for writing: %s\n",
                json_path,
                lsqa_strerror(errno, errbuf, sizeof(errbuf)));
        free(json_path);
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    collector.stream = fp;
    status = sixel_assessment_get_json(assessment,
                                       SIXEL_ASSESSMENT_SECTION_QUALITY,
                                       json_collect_callback,
                                       &collector);
    fclose(fp);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Failed to write JSON file: %s\n",
                sixel_helper_format_error(status));
        free(json_path);
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    collector.stream = stdout;
    collector.metrics = NULL;
    status = sixel_assessment_get_json(assessment,
                                       SIXEL_ASSESSMENT_SECTION_QUALITY,
                                       json_collect_callback,
                                       &collector);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "Failed to emit JSON: %s\n",
                sixel_helper_format_error(status));
        free(json_path);
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return LSQA_EXIT_RUNTIME_FAILED;
    }

    if (opts.verbose) {
        verbose_print(&metrics);
        fprintf(stderr, "\nWrote: %s\n", json_path);
    }

    baseline_status = lsqa_check_baseline(&opts, &metrics, &baseline_value);
    if (baseline_status != 0) {
        if (baseline_status < 0) {
            fprintf(stderr,
                    "Metric %s is unavailable\n",
                    opts.baseline_key);
            exit_code = LSQA_EXIT_RUNTIME_FAILED;
        } else if (baseline_status == 2) {
            fprintf(stderr,
                    "Metric %s is NaN\n",
                    opts.baseline_key);
            exit_code = LSQA_EXIT_BASELINE_FAILED;
        } else {
            fprintf(stderr,
                    "%s %.6f is below baseline %.6f\n",
                    opts.baseline_key,
                    baseline_value,
                    opts.baseline_value);
            exit_code = LSQA_EXIT_BASELINE_FAILED;
        }
    } else {
        exit_code = LSQA_EXIT_SUCCESS;
    }

    free(json_path);
    sixel_assessment_unref(assessment);
    sixel_frame_unref(ref_frame);
    sixel_frame_unref(out_frame);
    sixel_allocator_unref(allocator);
    return exit_code;
}
