/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_BODY_GPU_H
#define LIBSIXEL_ENCODER_BODY_GPU_H

#include <stddef.h>
#include <stdint.h>
#include <sixel.h>
#include <6cells.h>

/*
 * GPU band composition for the SIXEL body.  For a fully published indexed
 * frame it computes, per six-row band and palette register, the same color
 * spans the CPU compose builds (a gap of at most nine empty columns joins two
 * occupied runs) together with each span's six-bit column values.  Emission
 * stays on the CPU.  Nodes inside a band are listed by register then
 * column; first_seen gives each register's first row-major position in the
 * band (row * encoded_width + column), which callers use to reproduce the
 * CPU tie order between equal spans.
 */

/* Readable bytes past the end of every map, GPU or CPU, so emission may
 * load a short span as whole vectors. */
#define SIXEL_BODY_MAP_TAIL 32

/* One span: register, first column, one past the last column, and the
 * offset of its first column value in sixel_body_gpu_result_t.map. */
typedef struct sixel_body_gpu_node {
    uint32_t pal;
    uint32_t sx;
    uint32_t mx;
    uint32_t map_offset;
} sixel_body_gpu_node_t;

typedef struct sixel_body_gpu_request {
    unsigned char const *indexes;   /* source raster, width x height */
    unsigned char const *mask;      /* nonzero skips a source pixel; may be NULL */
    int width;
    int height;
    int encoded_width;
    int encoded_height;
    int offset_left;
    int offset_top;
    int ncolors;
    int keycolor;
    /* Optional shared storage behind indexes and mask; a run on the same
     * device binds it instead of copying. All may be NULL. */
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *indexes_buffer;
    sixel_gpu_buffer_interface_t *mask_buffer;
    /* Emission settings. A backend that spells the body also returns its
     * bytes: registers[c] is the number written after '#' for color c,
     * selectors is zero when no '#' is written (the two-color keyed rule),
     * and limited splits repeats above 255. */
    uint32_t const *registers;
    int selectors;
    int limited;
} sixel_body_gpu_request_t;

/* Borrowed views into the context's buffers, valid until the next run or
 * destroy.  Band b owns nodes[band_first[b] .. band_first[b + 1]). */
typedef struct sixel_body_gpu_result {
    sixel_body_gpu_node_t const *nodes;
    uint32_t const *band_first;
    unsigned char *map;
    uint32_t const *first_seen;     /* [band * ncolors + register] */
    int band_count;
    int ncolors;
    /* The body as the CPU emission would write it, or NULL when the backend
     * returns spans only. Band b is bytes[band_bytes[b] .. band_bytes[b + 1])
     * and begins with its '-' separator, except band 0. */
    unsigned char const *bytes;
    uint32_t const *band_bytes;
} sixel_body_gpu_result_t;

typedef struct sixel_body_gpu sixel_body_gpu_t;

#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
/* SIXEL_FEATURE_ERROR when no device is present. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_body_gpu_new(sixel_body_gpu_t **context);
SIXEL_INTERNAL_API void
sixel_body_gpu_destroy(sixel_body_gpu_t *context);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_body_gpu_run(sixel_body_gpu_t *context,
                   sixel_body_gpu_request_t const *request,
                   sixel_body_gpu_result_t *result);
#else
/* No GPU backend is built: callers keep the CPU compose. */
static inline SIXELSTATUS
sixel_body_gpu_new(sixel_body_gpu_t **context)
{
    *context = NULL;
    return SIXEL_FEATURE_ERROR;
}
static inline void
sixel_body_gpu_destroy(sixel_body_gpu_t *context)
{
    (void)context;
}
static inline SIXELSTATUS
sixel_body_gpu_run(sixel_body_gpu_t *context,
                   sixel_body_gpu_request_t const *request,
                   sixel_body_gpu_result_t *result)
{
    (void)context;
    (void)request;
    (void)result;
    return SIXEL_FEATURE_ERROR;
}
#endif

#endif
