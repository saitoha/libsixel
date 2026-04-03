/*
 * Helper exposing k-means tuning getters for TAP shell wrappers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "src/palette-kmeans.h"

/*
 * Keep this test independent from src/compat_stub.h.
 *
 * The compat layer is private to src/ and should not be used from tests.
 * Pick an environment setter based on configure/meson feature detection.
 */
static int
test_setenv(char const *name, char const *value)
{
#if defined(HAVE__PUTENV_S)
    return _putenv_s(name, value);
#elif defined(HAVE_SETENV)
    extern int setenv(char const *name, char const *value, int overwrite);

    return setenv(name, value, 1);
#else
    (void)name;
    (void)value;

    return -1;
#endif
}

static char const *
test_sixel_kmeans_init_type_to_string(sixel_kmeans_init_type init_type)
{
    switch (init_type) {
    case SIXEL_PALETTE_KMEANS_INIT_PCA:
        return "pca";
    case SIXEL_PALETTE_KMEANS_INIT_NONE:
        return "none";
    case SIXEL_PALETTE_KMEANS_INIT_AUTO:
    default:
        return "auto";
    }
}

static char const *
test_sixel_kmeans_binning_mode_to_string(sixel_kmeans_binning_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_BINNING_NONE:
        return "none";
    case SIXEL_PALETTE_KMEANS_BINNING_HARD:
        return "hard";
    case SIXEL_PALETTE_KMEANS_BINNING_SOFT:
        return "soft";
    case SIXEL_PALETTE_KMEANS_BINNING_AUTO:
    default:
        return "auto";
    }
}

static char const *
test_sixel_kmeans_mapping_mode_to_string(sixel_kmeans_mapping_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_MAPPING_SRGB:
        return "srgb";
    case SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM:
    default:
        return "uniform";
    }
}

static char const *
test_sixel_kmeans_softdist_mode_to_string(sixel_kmeans_softdist_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR:
    default:
        return "trilinear";
    }
}

static char const *
test_sixel_kmeans_feedback_mode_to_string(sixel_kmeans_feedback_mode mode)
{
    switch (mode) {
    case SIXEL_PALETTE_KMEANS_FEEDBACK_ON:
        return "on";
    case SIXEL_PALETTE_KMEANS_FEEDBACK_OFF:
    default:
        return "off";
    }
}

static void
test_print_kmeans_histogram_settings(void)
{
    printf("binning=%s binbits=%u mapping=%s softdist=%s autoratio=%u "
           "feedback=%s\n",
           test_sixel_kmeans_binning_mode_to_string(
               sixel_get_kmeans_binning_mode()),
           sixel_get_kmeans_binbits(),
           test_sixel_kmeans_mapping_mode_to_string(
               sixel_get_kmeans_mapping_mode()),
           test_sixel_kmeans_softdist_mode_to_string(
               sixel_get_kmeans_softdist_mode()),
           sixel_get_kmeans_autoratio(),
           test_sixel_kmeans_feedback_mode_to_string(
               sixel_get_kmeans_feedback_mode()));
}

int
test_palette_0001_kmeans_init(int argc, char **argv)
{
    sixel_kmeans_init_type first;
    sixel_kmeans_init_type second;
    int run_cache_check;
    int run_histogram;
    int run_histogram_override;

    run_cache_check = 0;
    run_histogram = 0;
    run_histogram_override = 0;
    if (argc > 1) {
        if (strcmp(argv[1], "--cache") == 0) {
            run_cache_check = 1;
        } else if (strcmp(argv[1], "--histogram") == 0) {
            run_histogram = 1;
        } else if (strcmp(argv[1], "--histogram-override") == 0) {
            run_histogram_override = 1;
        }
    }

    if (run_histogram != 0) {
        test_print_kmeans_histogram_settings();

        return 0;
    }

    if (run_histogram_override != 0) {
        sixel_set_kmeans_binning_mode_override(
            1,
            SIXEL_PALETTE_KMEANS_BINNING_HARD);
        sixel_set_kmeans_binbits_override(1, 7u);
        sixel_set_kmeans_mapping_mode_override(
            1,
            SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM);
        sixel_set_kmeans_softdist_mode_override(
            1,
            SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR);
        sixel_set_kmeans_autoratio_override(1, 17u);
        sixel_set_kmeans_feedback_mode_override(
            1,
            SIXEL_PALETTE_KMEANS_FEEDBACK_OFF);
        test_print_kmeans_histogram_settings();

        return 0;
    }

    first = sixel_get_kmeans_init_type();
    if (!run_cache_check) {
        printf("%s\n", test_sixel_kmeans_init_type_to_string(first));

        return 0;
    }

    if (test_setenv("SIXEL_PALETTE_KMEANS_INITTYPE", "none") != 0) {
        fprintf(stderr, "failed to set environment override\n");

        return 1;
    }

    second = sixel_get_kmeans_init_type();
    printf("%s %s\n",
           test_sixel_kmeans_init_type_to_string(first),
           test_sixel_kmeans_init_type_to_string(second));

    return 0;
}
