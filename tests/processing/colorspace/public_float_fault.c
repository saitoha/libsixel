/* SPDX-License-Identifier: MIT */
/* Exercise the real chunk controller with a deterministic short-lived pool. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <sixel.h>
#include <6cells.h>
#include "src/options.h"
#include "src/threading.h"

#if SIXEL_ENABLE_THREADS
static SIXELSTATUS float_fault_service(char const *name, void **out);
/* Only the private converter copy sees these allocation probes. The pool
 * below dispatches synchronously, so no shared counters enter production. */
static int float_fault_mode;
static int float_fault_memo_attempts;
static int float_fault_memo_frees;
static void *float_fault_memos[64];

static void *
float_fault_calloc(size_t count, size_t size)
{
    void *memory;

    if (float_fault_mode >= 4 && count == 16384U) {
        ++float_fault_memo_attempts;
        if (float_fault_mode == 5) {
            return NULL;
        }
    }
    memory = calloc(count, size);
    if (float_fault_mode >= 4 && count == 16384U &&
            float_fault_memo_attempts <= 64) {
        float_fault_memos[float_fault_memo_attempts - 1] = memory;
    }
    return memory;
}

static void
float_fault_free(void *memory)
{
    int i;

    if (float_fault_mode >= 4 && memory != NULL) {
        for (i = 0; i < 64; ++i) {
            if (float_fault_memos[i] == memory) {
                ++float_fault_memo_frees;
                float_fault_memos[i] = NULL;
                break;
            }
        }
    }
    free(memory);
}

/* The real converter also exists in an amalgamated test runner. Keep
 * every test-copy type, table and helper local to this include seam. */
#define sixel_float_memo_workspace cff_memo_6
#define sixel_float_memo_workspace_t cff_memo_7
#define sixel_float_memo_create cff_memo_8
#define sixel_float_memo_cleanup cff_memo_9
#define sixel_convert_pixels_via_linear_float_serial cff_memo_10
#define gamma_float_grid cff_memo_0
#define gamma_float_to_linear_lut cff_memo_1
#define sixel_float_memo_entry cff_memo_2
#define sixel_float_memo_entry_t cff_memo_3
#define sixel_float_memo_slot cff_memo_4
#define sixel_color_decode_srgb_float cff_memo_5
#define gamma_to_linear_double_lut cff_0
#define sixel_cbrt_lut cff_1
#define sixel_cbrt_lut_double cff_2
#define sixel_cielab_clamp_ab cff_3
#define sixel_cielab_decode_ab cff_4
#define sixel_cielab_encode_L cff_5
#define sixel_cielab_encode_ab cff_6
#define sixel_cielab_f cff_7
#define sixel_cielab_f_inv cff_8
#define sixel_cielab_to_din99d cff_9
#define sixel_cielab_to_linear cff_10
#define sixel_cielab_to_linear_impl cff_11
#define sixel_clamp_unit cff_12
#define sixel_color_lab_f cff_13
#define sixel_color_triplet_finite cff_14
#define sixel_colorspace_alpha_mask_neon cff_15
#define sixel_colorspace_alpha_mask_sse2 cff_16
#define sixel_colorspace_apply_neon cff_17
#define sixel_colorspace_apply_sse2 cff_18
#define sixel_colorspace_build_once cff_19
#define sixel_colorspace_build_tables cff_20
#define sixel_colorspace_cbrt_lookup_double cff_21
#define sixel_colorspace_clamp cff_22
#define sixel_colorspace_convert_component cff_23
#define sixel_colorspace_convert_neon cff_24
#define sixel_colorspace_convert_sse2 cff_25
#define sixel_colorspace_create_pool cff_26
#define sixel_colorspace_fill_neon_table cff_27
#define sixel_colorspace_init_tables cff_28
#define sixel_colorspace_log_clamp cff_29
#define sixel_colorspace_lookup_neon cff_30
#define sixel_colorspace_neon_supported_format cff_31
#define sixel_colorspace_once cff_32
#define sixel_colorspace_parallel_byte_context cff_33
#define sixel_colorspace_parallel_byte_context_t cff_34
#define sixel_colorspace_parallel_context cff_35
#define sixel_colorspace_parallel_context_t cff_36
#define sixel_colorspace_parallel_min_pixels cff_37
#define sixel_colorspace_parallel_min_pixels_cached cff_38
#define sixel_colorspace_parallel_min_pixels_init_once cff_39
#define sixel_colorspace_parallel_min_pixels_once cff_40
#define sixel_colorspace_parallel_min_pixels_once_cb cff_41
#define sixel_colorspace_parallel_worker cff_42
#define sixel_colorspace_parallel_worker_bytes cff_43
#define sixel_colorspace_prepare_neon_tables cff_44
#define sixel_colorspace_select_lut cff_45
#define sixel_colorspace_supports_byte_format cff_46
#define sixel_colorspace_supports_lut_pair cff_47
#define sixel_convert_pixels_via_linear cff_48
#define sixel_convert_pixels_via_linear_chunk cff_49
#define sixel_convert_pixels_via_linear_float cff_50
#define sixel_convert_pixels_via_linear_float_chunk cff_51
#define sixel_decode_linear_from_colorspace cff_52
#define sixel_din99d_clamp_ab cff_53
#define sixel_din99d_clamp_ab_norm cff_54
#define sixel_din99d_decode_ab cff_55
#define sixel_din99d_encode_L cff_56
#define sixel_din99d_encode_ab cff_57
#define sixel_din99d_to_cielab cff_58
#define sixel_din99d_to_linear cff_59
#define sixel_din99d_to_linear_impl cff_60
#define sixel_encode_linear_to_colorspace cff_61
#define sixel_linear_double_to_byte cff_62
#define sixel_linear_double_to_smptec cff_63
#define sixel_linear_double_to_srgb cff_64
#define sixel_linear_smptec_to_srgb cff_65
#define sixel_linear_smptec_to_srgb_matrix cff_66
#define sixel_linear_srgb_to_smptec cff_67
#define sixel_linear_srgb_to_smptec_matrix cff_68
#define sixel_linear_to_cielab cff_69
#define sixel_linear_to_cielab_impl cff_70
#define sixel_linear_to_din99d cff_71
#define sixel_linear_to_din99d_impl cff_72
#define sixel_linear_to_oklab cff_73
#define sixel_linear_to_oklab_impl cff_74
#define sixel_neon_gamma_to_linear cff_75
#define sixel_neon_linear_to_gamma cff_76
#define sixel_neon_tables_initialized cff_77
#define sixel_oklab_decode_ab cff_78
#define sixel_oklab_encode_L cff_79
#define sixel_oklab_encode_ab cff_80
#define sixel_oklab_to_linear cff_81
#define sixel_oklab_to_linear_impl cff_82
#define sixel_pixelformat_layout cff_83
#define sixel_pixelformat_layout_init cff_84
#define sixel_pixelformat_layout_t cff_85
#define sixel_smptec_to_linear_double cff_86
#define sixel_srgb_to_linear_double cff_87
#define smptec_to_linear_double_lut cff_88
#define tables_initialized cff_89
#define sixel_colorspace_supports_pixelformat fault_supports_pixelformat
#define sixel_helper_convert_colorspace fault_convert_colorspace
#define sixel_color_decode_srgb fault_color_decode_srgb
#define sixel_color_encode_srgb fault_color_encode_srgb
#define sixel_color_convert fault_color_convert
#define sixel_color_convert_output fault_color_convert_output
#define sixel_color_from_output fault_color_from_output
#define sixel_color_convert_rgbf32 fault_color_convert_rgbf32
#define sixel_color_convert_frame_rgbf32 fault_color_convert_frame_rgbf32
#define gamma_to_linear_lut cff_gamma_to_linear_lut
#define linear_to_gamma_lut cff_linear_to_gamma_lut
#define linear_to_smptec_lut cff_linear_to_smptec_lut
#define sixel_components_getservice float_fault_service
/* The ordinary header can already be guarded in an amalgamated build. */
double sixel_color_decode_srgb(double value);
int sixel_color_convert(double out[3], double const in[3],
    int source, int target);
int sixel_color_convert_output(double out[3], double const in[3],
    int source, int target);
int sixel_color_from_output(double out[3], double const in[3],
    int source, int target);
#define calloc float_fault_calloc
#define free float_fault_free
#include "src/colorspace.c"
#undef free
#undef calloc
#undef sixel_components_getservice
#undef linear_to_smptec_lut
#undef linear_to_gamma_lut
#undef gamma_to_linear_lut
#undef sixel_float_memo_workspace
#undef sixel_float_memo_workspace_t
#undef sixel_float_memo_create
#undef sixel_float_memo_cleanup
#undef sixel_convert_pixels_via_linear_float_serial
#undef gamma_float_grid
#undef gamma_float_to_linear_lut
#undef sixel_float_memo_entry
#undef sixel_float_memo_entry_t
#undef sixel_float_memo_slot
#undef sixel_color_decode_srgb_float
#undef gamma_to_linear_double_lut
#undef sixel_cbrt_lut
#undef sixel_cbrt_lut_double
#undef sixel_cielab_clamp_ab
#undef sixel_cielab_decode_ab
#undef sixel_cielab_encode_L
#undef sixel_cielab_encode_ab
#undef sixel_cielab_f
#undef sixel_cielab_f_inv
#undef sixel_cielab_to_din99d
#undef sixel_cielab_to_linear
#undef sixel_cielab_to_linear_impl
#undef sixel_clamp_unit
#undef sixel_color_lab_f
#undef sixel_color_triplet_finite
#undef sixel_colorspace_alpha_mask_neon
#undef sixel_colorspace_alpha_mask_sse2
#undef sixel_colorspace_apply_neon
#undef sixel_colorspace_apply_sse2
#undef sixel_colorspace_build_once
#undef sixel_colorspace_build_tables
#undef sixel_colorspace_cbrt_lookup_double
#undef sixel_colorspace_clamp
#undef sixel_colorspace_convert_component
#undef sixel_colorspace_convert_neon
#undef sixel_colorspace_convert_sse2
#undef sixel_colorspace_create_pool
#undef sixel_colorspace_fill_neon_table
#undef sixel_colorspace_init_tables
#undef sixel_colorspace_log_clamp
#undef sixel_colorspace_lookup_neon
#undef sixel_colorspace_neon_supported_format
#undef sixel_colorspace_once
#undef sixel_colorspace_parallel_byte_context
#undef sixel_colorspace_parallel_byte_context_t
#undef sixel_colorspace_parallel_context
#undef sixel_colorspace_parallel_context_t
#undef sixel_colorspace_parallel_min_pixels
#undef sixel_colorspace_parallel_min_pixels_cached
#undef sixel_colorspace_parallel_min_pixels_init_once
#undef sixel_colorspace_parallel_min_pixels_once
#undef sixel_colorspace_parallel_min_pixels_once_cb
#undef sixel_colorspace_parallel_worker
#undef sixel_colorspace_parallel_worker_bytes
#undef sixel_colorspace_prepare_neon_tables
#undef sixel_colorspace_select_lut
#undef sixel_colorspace_supports_byte_format
#undef sixel_colorspace_supports_lut_pair
#undef sixel_convert_pixels_via_linear
#undef sixel_convert_pixels_via_linear_chunk
#undef sixel_convert_pixels_via_linear_float
#undef sixel_convert_pixels_via_linear_float_chunk
#undef sixel_decode_linear_from_colorspace
#undef sixel_din99d_clamp_ab
#undef sixel_din99d_clamp_ab_norm
#undef sixel_din99d_decode_ab
#undef sixel_din99d_encode_L
#undef sixel_din99d_encode_ab
#undef sixel_din99d_to_cielab
#undef sixel_din99d_to_linear
#undef sixel_din99d_to_linear_impl
#undef sixel_encode_linear_to_colorspace
#undef sixel_linear_double_to_byte
#undef sixel_linear_double_to_smptec
#undef sixel_linear_double_to_srgb
#undef sixel_linear_smptec_to_srgb
#undef sixel_linear_smptec_to_srgb_matrix
#undef sixel_linear_srgb_to_smptec
#undef sixel_linear_srgb_to_smptec_matrix
#undef sixel_linear_to_cielab
#undef sixel_linear_to_cielab_impl
#undef sixel_linear_to_din99d
#undef sixel_linear_to_din99d_impl
#undef sixel_linear_to_oklab
#undef sixel_linear_to_oklab_impl
#undef sixel_neon_gamma_to_linear
#undef sixel_neon_linear_to_gamma
#undef sixel_neon_tables_initialized
#undef sixel_oklab_decode_ab
#undef sixel_oklab_encode_L
#undef sixel_oklab_encode_ab
#undef sixel_oklab_to_linear
#undef sixel_oklab_to_linear_impl
#undef sixel_pixelformat_layout
#undef sixel_pixelformat_layout_init
#undef sixel_pixelformat_layout_t
#undef sixel_smptec_to_linear_double
#undef sixel_srgb_to_linear_double
#undef smptec_to_linear_double_lut
#undef tables_initialized
#undef sixel_colorspace_supports_pixelformat
#undef sixel_helper_convert_colorspace
#undef sixel_color_decode_srgb
#undef sixel_color_encode_srgb
#undef sixel_color_convert
#undef sixel_color_convert_output
#undef sixel_color_from_output
#undef sixel_color_convert_rgbf32
#undef sixel_color_convert_frame_rgbf32

static sixel_thread_pool_create_request_t float_fault_request;
static int float_fault_pushes;
static int float_fault_created;
static void *float_fault_workspaces[7];
static int float_fault_workers;
static int float_fault_next;
static int float_fault_grows;
static int float_fault_used;


static void
float_fault_unref(sixel_thread_pool_t *self)
{
    int i;
    cff_memo_7 *state;

    (void)self;
    if (float_fault_mode >= 4) {
        for (i = 0; i < float_fault_workers; ++i) {
            state = (cff_memo_7 *)float_fault_workspaces[i];
            float_fault_used += state != NULL && state->initialized;
            if (state != NULL &&
                    float_fault_request.workspace_cleanup != NULL) {
                float_fault_request.workspace_cleanup(state);
            }
            free(state);
            float_fault_workspaces[i] = NULL;
        }
    }
}

static void
float_fault_finish(sixel_thread_pool_t *self)
{
    (void)self;
}

static int
float_fault_grow(sixel_thread_pool_t *self, int additional)
{
    int i;

    (void)self;
    ++float_fault_grows;
    for (i = 0; i < additional && float_fault_workers < 7; ++i) {
        float_fault_workspaces[float_fault_workers++] =
            calloc(1U, float_fault_request.workspace_size);
    }
    return SIXEL_OK;
}

static SIXELSTATUS
float_fault_push(sixel_thread_pool_t *self, sixel_thread_pool_job_t job)
{
    (void)self;
    ++float_fault_pushes;
    if (float_fault_mode == 1 && job.band_index == 1) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (float_fault_mode >= 4) {
        float_fault_next %= float_fault_workers;
        return float_fault_request.worker(job, float_fault_request.userdata,
            float_fault_workspaces[float_fault_next++]);
    }
    return float_fault_request.worker(job, float_fault_request.userdata,
        NULL);
}

static int
float_fault_error(sixel_thread_pool_t *self)
{
    (void)self;
    return float_fault_mode == 3 ? SIXEL_RUNTIME_ERROR : SIXEL_OK;
}

static sixel_thread_pool_vtbl_t const float_fault_vtbl = {
    NULL, float_fault_unref, NULL, float_fault_push, float_fault_finish,
    float_fault_error, float_fault_grow
};
static sixel_thread_pool_t float_fault_pool = {&float_fault_vtbl};

static SIXELSTATUS
float_fault_create(sixel_threadpool_service_t *self,
    sixel_thread_pool_create_request_t const *request,
    sixel_thread_pool_t **out)
{
    (void)self;
    ++float_fault_created;
    if (float_fault_mode == 2) {
        *out = NULL;
        return SIXEL_BAD_ALLOCATION;
    }
    float_fault_request = *request;
    if (float_fault_mode >= 4) {
        if (request->workspace_size != sizeof(cff_memo_7) ||
                request->workspace_cleanup == NULL) {
            *out = NULL;
            return SIXEL_BAD_ALLOCATION;
        }
        (void)float_fault_grow(&float_fault_pool, request->threads);
        float_fault_grows = 0;
    }
    *out = &float_fault_pool;
    return SIXEL_OK;
}

static SIXELSTATUS
float_fault_service(char const *name, void **out)
{
    static sixel_threadpool_service_vtbl_t const vtbl = {
        NULL, NULL, NULL, float_fault_create
    };
    static sixel_threadpool_service_t service = {&vtbl};

    (void)name;
    *out = &service;
    return SIXEL_OK;
}
#endif

int
test_color_public_float_fault(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    float pixels[9];
    sixel_runtime_policy_options_t runtime;
    SIXELSTATUS status;
    int i;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    float_fault_mode = atoi(argv[1]);
    float_fault_created = 0;
    float_fault_pushes = 0;
    sixel_runtime_policy_load(&runtime);
    runtime.colorspace_parallel_min_pixels = 1;
    runtime.colorspace_parallel_min_pixels_override = 1;
    sixel_runtime_policy_store(&runtime);
    sixel_set_encoder_threads(3);
    for (i = 0; i < 9; ++i) {
        pixels[i] = .25f;
    }
    status = fault_convert_colorspace((unsigned char *)pixels,
        sizeof(pixels), SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_GAMMA);
    if (float_fault_created != 1 || float_fault_pushes !=
            (float_fault_mode == 2 ? 0 : 2) ||
            status != (float_fault_mode == 3 ? SIXEL_RUNTIME_ERROR :
                SIXEL_OK)) {
        return EXIT_FAILURE;
    }
    /* Queue/create fallback converts once; pool errors publish nothing.
     * Double conversion would give about .759 instead of .537. */
    for (i = 0; i < 9; ++i) {
        if (!isfinite(pixels[i]) ||
                fabs(pixels[i] - (float_fault_mode == 3 ? .25 :
                    .53709873048319425)) > 2e-7) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
#else
    (void)argc;
    (void)argv;
    return EXIT_FAILURE;
#endif
}

#if SIXEL_ENABLE_THREADS
static int
float_fault_donate(void *context)
{
    int *polls;

    polls = (int *)context;
    return ++*polls == 2;
}
#endif

/* Assert bounded allocation and cleanup with deterministic worker reuse,
 * then repeat with every optional memo allocation rejected. The real-pool
 * numerical and growth path is owned by parallel/0012_worker_memo_exact. */
int
test_color_worker_memo_lifetime(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    float *pixels;
    double input[3];
    double output[3];
    float expected[3];
    sixel_thread_budget_scope_t scope;
    sixel_runtime_policy_options_t runtime;
    SIXELSTATUS status;
    size_t count;
    size_t i;
    int channel;
    int target;
    int polls;
    int ok;

    (void)argc;
    (void)argv;
    count = 262145U;
    pixels = (float *)malloc(count * 3U * sizeof(float));
    if (pixels == NULL) {
        return EXIT_FAILURE;
    }
    sixel_runtime_policy_load(&runtime);
    runtime.colorspace_parallel_min_pixels = 1;
    runtime.colorspace_parallel_min_pixels_override = 1;
    sixel_runtime_policy_store(&runtime);
    ok = 1;
    for (float_fault_mode = 4; float_fault_mode <= 5; ++float_fault_mode) {
        for (target = SIXEL_COLORSPACE_LINEAR;
                target <= SIXEL_COLORSPACE_CIELAB; target += 3) {
            for (i = 0U; i < count * 3U; ++i) {
                pixels[i] = .25f;
            }
            for (channel = 0; channel < 3; ++channel) {
                input[channel] = .25;
            }
            ok &= fault_color_convert(output, input,
                SIXEL_COLORSPACE_GAMMA, target);
            for (channel = 0; channel < 3; ++channel) {
                expected[channel] = (float)output[channel];
            }
            float_fault_memo_attempts = 0;
            float_fault_memo_frees = 0;
            float_fault_workers = 0;
            float_fault_next = 0;
            float_fault_used = 0;
            memset(float_fault_memos, 0, sizeof(float_fault_memos));
            polls = 0;
            sixel_set_encoder_threads(8);
            sixel_thread_budget_enter(&scope, 4);
            sixel_thread_budget_set_reclaim(&scope, 8,
                float_fault_donate, &polls);
            status = fault_color_convert_frame_rgbf32(pixels, count,
                SIXEL_COLORSPACE_GAMMA, target);
            sixel_thread_budget_leave(&scope);
            ok &= status == SIXEL_OK && polls == 2 &&
                float_fault_grows == 1 && float_fault_used == 7 &&
                float_fault_memo_attempts == 8 &&
                float_fault_memo_frees == (float_fault_mode == 4 ? 8 : 0);
            for (i = 0U; i < count && ok; ++i) {
                ok &= memcmp(pixels + i * 3U, expected,
                    sizeof(expected)) == 0;
            }
        }
    }
    float_fault_mode = 0;
    free(pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    (void)argc;
    (void)argv;
    return EXIT_FAILURE;
#endif
}
