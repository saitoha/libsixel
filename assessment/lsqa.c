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
#if HAVE_MATH_H
# include <math.h>
#endif

#include <sixel.h>

#include "assessment.h"

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

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &capture);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_loader_load_file(loader,
                                    path,
                                    capture_first_frame);
    if (SIXEL_FAILED(status) || capture.frame == NULL) {
        fprintf(stderr,
                "libsixel loader failed for %s: %s\n",
                path,
                sixel_helper_format_error(status));
        goto error;
    }

    sixel_loader_unref(loader);
    *out_frame = capture.frame;
    return 0;

error:
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

static int
metric_name_matches(const char *input, const char *target)
{
    if (input == NULL || target == NULL) {
        return 0;
    }
#if HAVE_STRINGS_H
    if (strcasecmp(input, target) == 0) {
        return 1;
    }
#endif
    if (strcmp(input, target) == 0) {
        return 1;
    }
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

typedef struct Options {
    const char *ref_path;
    const char *out_path;
    const char *prefix;
    char prefix_buffer[PATH_MAX];
    const MetricSpec *metric_spec;
    const char *metric_key;
    unsigned int metric_mask;
    int metrics_filtered;
    int verbose;
} Options;

static void
print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-m NAME] <reference> [output]\n",
            prog);
    fprintf(stderr,
            "       %s [-m NAME] <reference> < output\n",
            prog);
    fprintf(stderr,
            "       %s [-m NAME] <reference> - < output\n",
            prog);
    fprintf(stderr,
            "  -m, --metrics NAME  limit computation to a single metric\n");
    fprintf(stderr,
            "                        and print only that value\n");
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
    const MetricSpec *metric_spec;
    size_t prefix_len;
    int status;
    int argi;

    opts->ref_path = NULL;
    opts->out_path = "-";
    opts->prefix = "report";
    opts->metric_spec = NULL;
    opts->metric_key = NULL;
    opts->metric_mask = SIXEL_ASSESSMENT_METRIC_MASK_ALL;
    opts->metrics_filtered = 0;
    opts->verbose = 0;
    opts->prefix_buffer[0] = '\0';

    argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        const char *arg;

        arg = argv[argi];
        if (strcmp(arg, "-m") == 0 || strcmp(arg, "--metrics") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "-m requires a metric name\n");
                return -1;
            }
            metrics_arg = argv[argi + 1];
            metric_spec = metric_spec_from_name(metrics_arg);
            if (metric_spec == NULL) {
                fprintf(stderr, "Unknown metric: %s\n", metrics_arg);
                return -1;
            }
            if (opts->metric_spec != NULL) {
                fprintf(stderr, "Metric already specified\n");
                return -1;
            }
            opts->metric_spec = metric_spec;
            opts->metric_key = metric_spec->json_key;
            opts->metric_mask = SIXEL_ASSESSMENT_METRIC_MASK(
                metric_spec->metric_id);
            opts->metrics_filtered = 1;
            argi += 2;
            continue;
        }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            return -1;
        }
        break;
    }

    if (argc - argi < 1 || argc - argi > 2) {
        fprintf(stderr, "Invalid number of arguments\n");
        return -1;
    }

    ref_arg = argv[argi];
    opts->ref_path = ref_arg;
    argi += 1;

    if (argc - argi >= 1) {
        out_arg = argv[argi];
        if (strcmp(out_arg, "-") == 0) {
            opts->out_path = "-";
        } else {
            opts->out_path = out_arg;
        }
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
            return -1;
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
            free(prefix_env);
            return -1;
        }
        opts->prefix = opts->prefix_buffer;
    }

    verbose_env = lsqa_getenv_dup("LSQA_VERBOSE");
    if (verbose_env != NULL && verbose_env[0] != '\0') {
        opts->verbose = 1;
    }

    free(prefix_env);
    free(verbose_env);

    return 0;
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

    allocator = NULL;
    assessment = NULL;
    ref_frame = NULL;
    out_frame = NULL;
    fp = NULL;
    json_path = NULL;
    exit_code = EXIT_FAILURE;
    metric_status = 0;
    metric_value = 0.0f;

    if (parse_args(argc, argv, &opts) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    if (load_frame(opts.ref_path, allocator, &ref_frame) != 0) {
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }
    if (load_frame(opts.out_path, allocator, &out_frame) != 0) {
        sixel_frame_unref(ref_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
    }

    status = sixel_assessment_new(&assessment, allocator);
    if (SIXEL_FAILED(status) || assessment == NULL) {
        fprintf(stderr,
                "Failed to create assessment object: %s\n",
                sixel_helper_format_error(status));
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    metrics_init(&metrics);
    collector.metrics = &metrics;

    if (opts.metrics_filtered) {
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
            return EXIT_FAILURE;
        }
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
            return EXIT_FAILURE;
        }
        if (metric_status > 0) {
            fprintf(stdout, "nan\n");
        } else {
            fprintf(stdout, "%.6f\n", metric_value);
        }
        if (opts.verbose) {
            verbose_print(&metrics);
        }
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_SUCCESS;
    }

    path_len = strlen(opts.prefix) + strlen("_metrics.json") + 1u;
    json_path = (char *)malloc(path_len);
    if (json_path == NULL) {
        fprintf(stderr, "Out of memory while building JSON path\n");
        sixel_assessment_unref(assessment);
        sixel_frame_unref(ref_frame);
        sixel_frame_unref(out_frame);
        sixel_allocator_unref(allocator);
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    if (opts.verbose) {
        verbose_print(&metrics);
        fprintf(stderr, "\nWrote: %s\n", json_path);
    }

    free(json_path);
    sixel_assessment_unref(assessment);
    sixel_frame_unref(ref_frame);
    sixel_frame_unref(out_frame);
    sixel_allocator_unref(allocator);
    exit_code = EXIT_SUCCESS;
    return exit_code;
}
