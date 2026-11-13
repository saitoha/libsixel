/* SPDX-License-Identifier: MIT */

enum { SIXEL_GPU_METAL_EYTZINGER_KEY_MAX = 255 * 3 };
/*
 * The GPU lookup uses the same one-dimensional projection as the CPU
 * Eytzinger policy.  The sorted projection and its integer-key lower-bound
 * table are kept separate so each pixel can start at a rank without walking
 * a divergent tree.
 */
typedef struct sixel_gpu_metal_eytzinger_entry {
    float key;
    uint32_t palette_index;
} sixel_gpu_metal_eytzinger_entry_t;

typedef struct sixel_gpu_metal_eytzinger_params {
    uint32_t count;
    uint32_t window;
} sixel_gpu_metal_eytzinger_params_t;

typedef struct sixel_gpu_metal_eytzinger_pair {
    float key;
    uint32_t palette_index;
} sixel_gpu_metal_eytzinger_pair_t;

static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_eytzinger_sorted_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_eytzinger_rank_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_eytzinger_params_buffer = { nil, 0U };
static int
sixel_gpu_metal_eytzinger_compare(void const *left, void const *right)
{
    sixel_gpu_metal_eytzinger_pair_t const *a;
    sixel_gpu_metal_eytzinger_pair_t const *b;

    a = (sixel_gpu_metal_eytzinger_pair_t const *)left;
    b = (sixel_gpu_metal_eytzinger_pair_t const *)right;
    if (a->key < b->key) {
        return -1;
    }
    if (a->key > b->key) {
        return 1;
    }
    return 0;
}

static void
sixel_gpu_metal_eytzinger_build(
    unsigned char const *palette,
    int ncolors,
    sixel_gpu_metal_eytzinger_entry_t *sorted,
    uint32_t *rank_lut,
    sixel_gpu_metal_eytzinger_params_t *params)
{
    sixel_gpu_metal_eytzinger_pair_t pairs[SIXEL_PALETTE_MAX];
    int index;
    int key;
    int rank;

    memset(sorted,
           0,
           (size_t)SIXEL_PALETTE_MAX
           * sizeof(sixel_gpu_metal_eytzinger_entry_t));
    memset(rank_lut,
           0,
           (size_t)(SIXEL_GPU_METAL_EYTZINGER_KEY_MAX + 1)
           * sizeof(rank_lut[0]));
    memset(params, 0, sizeof(*params));
    for (index = 0; index < ncolors; ++index) {
        pairs[index].key = (float)palette[index * 3 + 0]
                         + (float)palette[index * 3 + 1]
                         + (float)palette[index * 3 + 2];
        pairs[index].palette_index = (uint32_t)index;
    }
    qsort(pairs,
          (size_t)ncolors,
          sizeof(pairs[0]),
          sixel_gpu_metal_eytzinger_compare);
    for (index = 0; index < ncolors; ++index) {
        sorted[index].key = pairs[index].key;
        sorted[index].palette_index = pairs[index].palette_index;
    }
    for (key = 0; key <= SIXEL_GPU_METAL_EYTZINGER_KEY_MAX; ++key) {
        rank = ncolors - 1;
        for (index = 0; index < ncolors; ++index) {
            if ((float)key <= sorted[index].key) {
                rank = index;
                break;
            }
        }
        rank_lut[key] = (uint32_t)rank;
    }
    params->count = (uint32_t)ncolors;
    params->window = 6U;
}

static SIXELSTATUS
eytzinger_metal_prepare(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_params_t *params)
{
    sixel_gpu_metal_eytzinger_entry_t sorted[SIXEL_PALETTE_MAX];
    uint32_t rank[SIXEL_GPU_METAL_EYTZINGER_KEY_MAX + 1];
    sixel_gpu_metal_eytzinger_params_t configuration;

    (void)params;
    sixel_gpu_metal_eytzinger_build(request->palette, request->ncolors,
        sorted, rank, &configuration);
    run->buffers[0] = sixel_gpu_metal_upload_buffer(
        &g_sixel_gpu_metal_eytzinger_rank_buffer, rank, sizeof(rank));
    run->buffers[1] = sixel_gpu_metal_upload_buffer(
        &g_sixel_gpu_metal_eytzinger_sorted_buffer, sorted,
        (size_t)request->ncolors * sizeof(sorted[0]));
    run->buffers[2] = sixel_gpu_metal_upload_buffer(
        &g_sixel_gpu_metal_eytzinger_params_buffer, &configuration,
        sizeof(configuration));
    if (run->buffers[0] == nil || run->buffers[1] == nil ||
            run->buffers[2] == nil) return SIXEL_BAD_ALLOCATION;
    run->kernel = "eytzinger";
    run->kernel_id = 1;
    return SIXEL_OK;
}
static char const *const eytzinger_chunks[] = {
#include <generated-metal/lookup-policy-eytzinger-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-eytzinger-gpu-apply-shader.inc.h>
NULL
};

static gpu_metal_lookup_t const eytzinger_metal = {
    eytzinger_chunks, "lookup_eytzinger_apply",
    eytzinger_metal_prepare, NULL, NULL
};

gpu_metal_lookup_t const *
sixel_lookup_eytzinger_gpu_metal(void)
{
    return &eytzinger_metal;
}
