/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "bit-scan.h"
#include "simd-config.h"
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
# include <arm_neon.h>
#endif
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include "decoder-gpu.h"
#include "decoder-prescan.h"
#include "gpu-program.h"
#include "gpu-runtime-metal.h"

static char const *const decoder_gpu_source[] = {
#include "generated-metal/decoder-gpu-shader.inc.h"
};

typedef struct decoder_gpu_segment {
    uint32_t start, end, band, seg;
} decoder_gpu_segment_t;

typedef struct decoder_gpu_params {
    uint32_t segments, width, height, bands, initial_color;
} decoder_gpu_params_t;

enum {
    DECODER_GPU_NEEDS_CPU = 1u,
    DECODER_GPU_OUTSIDE = 2u,
    DECODER_GPU_TRANSPARENT = 4u
};

/* Chunk tags keep 24 bits of order above the 8-bit color. */
#define DECODER_GPU_MAX_SEGMENTS ((1u << 24) - 1u)

/* A thread per segment leaves the device mostly idle, since a frame has
 * only about a thousand of them; long segments are cut into chunks of
 * about this many bytes so their parses overlap. */
#define DECODER_GPU_CHUNK_BYTES SIXEL_PRESCAN_CHUNK_BYTES

/* One process-wide parser; a caller that finds it busy parses on the CPU. */
static pthread_mutex_t g_decoder_gpu_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_decoder_gpu_state;
static id<MTLComputePipelineState> g_decoder_gpu_parse;
static id<MTLComputePipelineState> g_decoder_gpu_pack;
static id<MTLComputePipelineState> g_decoder_gpu_measure;
static id<MTLCommandQueue> g_decoder_gpu_queue;
static id<MTLBuffer> g_decoder_gpu_body;
static id<MTLBuffer> g_decoder_gpu_segments;
static id<MTLBuffer> g_decoder_gpu_pixels;
static id<MTLBuffer> g_decoder_gpu_columns;
static id<MTLBuffer> g_decoder_gpu_flags;
static id<MTLBuffer> g_decoder_gpu_info;

static int
decoder_gpu_prepare(void)
{
    sixel_gpu_device_interface_t *device;
    id<MTLDevice> native;
    id<MTLLibrary> library;
    id<MTLFunction> parse;
    id<MTLFunction> pack;
    id<MTLFunction> measure;
    NSMutableString *source;
    size_t chunk;

    if (g_decoder_gpu_state != 0)
        return g_decoder_gpu_state > 0;
    g_decoder_gpu_state = -1;
    if (sixel_gpu_device_acquire(&device) != SIXEL_OK)
        return 0;
    native = sixel_gpu_metal_device(device);
    if (native == nil)
        return 0;
    @autoreleasepool {
        source = [NSMutableString string];
        for (chunk = 0; chunk < sizeof(decoder_gpu_source) /
                sizeof(decoder_gpu_source[0]); ++chunk) {
            [source appendString:[NSString stringWithUTF8String:
                decoder_gpu_source[chunk]]];
        }
        library = [native newLibraryWithSource:source options:nil error:NULL];
        parse = [library newFunctionWithName:@"decoder_gpu_parse"];
        pack = [library newFunctionWithName:@"decoder_gpu_pack"];
        measure = [library newFunctionWithName:@"decoder_gpu_measure"];
        if (parse != nil && pack != nil && measure != nil) {
            g_decoder_gpu_measure =
                [native newComputePipelineStateWithFunction:measure error:NULL];
            g_decoder_gpu_parse =
                [native newComputePipelineStateWithFunction:parse error:NULL];
            g_decoder_gpu_pack =
                [native newComputePipelineStateWithFunction:pack error:NULL];
        }
        [parse release];
        [pack release];
        [measure release];
        [library release];
    }
    g_decoder_gpu_queue = [native newCommandQueue];
    g_decoder_gpu_flags = [native newBufferWithLength:sizeof(uint32_t)
        options:MTLResourceStorageModeShared];
    if (g_decoder_gpu_parse == nil || g_decoder_gpu_pack == nil ||
            g_decoder_gpu_measure == nil ||
            g_decoder_gpu_queue == nil || g_decoder_gpu_flags == nil)
        return 0;
    g_decoder_gpu_state = 1;
    return 1;
}

/* Keep one device buffer of at least LENGTH bytes. */
static int
decoder_gpu_reserve(id<MTLBuffer> *buffer, NSUInteger length,
                    MTLResourceOptions options)
{
    id<MTLDevice> native;

    if (*buffer != nil && [*buffer length] >= length)
        return 1;
    native = [g_decoder_gpu_queue device];
    [*buffer release];
    *buffer = [native newBufferWithLength:length options:options];
    return *buffer != nil;
}

static void
decoder_gpu_add_chunk(decoder_gpu_segment_t *segments, uint32_t *count,
                      uint32_t start, uint32_t end, uint32_t band,
                      uint32_t seg)
{
    segments[*count].start = start;
    segments[*count].end = end;
    segments[*count].band = band;
    segments[*count].seg = seg;
    ++*count;
}

/* Split BODY[0, LENGTH) at '$' and '-', cut long segments into chunks, and
 * stop at the string terminator.  Returns the chunk count, or 0 when the
 * body needs the CPU parser. */
/* Accel: DECODE-BODY-SPLIT neon */
static uint32_t
decoder_gpu_split(unsigned char const *body, uint32_t length,
                  decoder_gpu_segment_t *segments, uint32_t capacity,
                  uint32_t *end, uint32_t *bands)
{
    uint32_t count;
    uint32_t band;
    uint32_t seg;
    uint32_t start;
    uint32_t p;
    uint32_t i;
    unsigned char ch;
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
    uint8x16_t dollar;
    uint8x16_t dash;
    uint8x16_t escape;
    uint8x16_t low;
    uint8x16_t high;
    uint8x16_t v;
    uint64_t hits;
    uint64_t data;
#endif

    count = 0u;
    band = 0u;
    seg = 0u;
    start = 0u;
    p = 0u;
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
    dollar = vdupq_n_u8('$');
    dash = vdupq_n_u8('-');
    escape = vdupq_n_u8(0x1b);
    low = vdupq_n_u8(63);
    high = vdupq_n_u8(126);
    for (; p + 16u <= length; p += 16u) {
        v = vld1q_u8(body + p);
        hits = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(
            vreinterpretq_u16_u8(vorrq_u8(vorrq_u8(vceqq_u8(v, dollar),
                vceqq_u8(v, dash)), vceqq_u8(v, escape))), 4)), 0);
        while (hits != 0u) {
            i = p + (sixel_bit_scan_forward64(hits) >> 2);
            hits &= ~(UINT64_C(0xf) << ((i - p) * 4u));
            ch = body[i];
            if (ch == 0x1b) {
                if (i + 1u < length && body[i + 1u] == '\\')
                    goto terminated;
                continue;
            }
            if (count + 1u >= capacity)
                return 0u;
            decoder_gpu_add_chunk(segments, &count, start, i, band, seg++);
            band += ch == '-';
            start = i + 1u;
        }
        /* Cut after the block's last sixel byte, which ends a token. */
        if (p + 16u - start >= DECODER_GPU_CHUNK_BYTES) {
            data = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(
                vreinterpretq_u16_u8(vandq_u8(vcgeq_u8(v, low),
                    vcleq_u8(v, high))), 4)), 0);
            if (data != 0u) {
                i = p + (sixel_bit_scan_reverse64(data) >> 2);
                if (i >= start) {
                    if (count + 1u >= capacity)
                        return 0u;
                    decoder_gpu_add_chunk(segments, &count, start, i + 1u,
                                          band, seg);
                    start = i + 1u;
                }
            }
        }
    }
#endif
    for (; p < length; ++p) {
        ch = body[p];
        if (ch == 0x1b) {
            if (p + 1u < length && body[p + 1u] == '\\') {
                i = p;
                goto terminated;
            }
        } else if (ch == '$' || ch == '-') {
            if (count + 1u >= capacity)
                return 0u;
            decoder_gpu_add_chunk(segments, &count, start, p, band, seg++);
            band += ch == '-';
            start = p + 1u;
        } else if (ch >= 63u && ch <= 126u &&
                p + 1u - start >= DECODER_GPU_CHUNK_BYTES) {
            if (count + 1u >= capacity)
                return 0u;
            decoder_gpu_add_chunk(segments, &count, start, p + 1u, band, seg);
            start = p + 1u;
        }
    }
    i = length;
terminated:
    /* Bytes after the terminator take the legacy CPU path. */
    if (i < length && i + 2u < length)
        return 0u;
    decoder_gpu_add_chunk(segments, &count, start, i, band, seg);
    *end = i;
    *bands = band + 1u;
    return count;
}

/* The caller scanned from the DCS final byte, while the workers start
 * after the prefix parsed by the CPU. Translate the offsets once here. */
static uint32_t
decoder_gpu_rebase(sixel_decode_prescan_t const *plan,
                   unsigned char const *input, size_t length, size_t body,
                   decoder_gpu_segment_t *segments, uint32_t capacity,
                   uint32_t *end, uint32_t *bands)
{
    unsigned char const *final;
    uint32_t const *cuts;
    size_t prefix;
    size_t cut_count;
    size_t i;
    uint32_t band;
    uint32_t seg;
    uint32_t start;
    uint32_t offset;
    uint32_t kind;
    uint32_t count;
    uint32_t bias;

    if (length < 5u || body < 2u || input[length - 2u] != 0x1bu ||
            input[length - 1u] != '\\')
        return 0u;
    if (input[0] == 0x1bu && input[1] == 'P')
        prefix = 2u;
    else if (input[0] == 0x90u)
        prefix = 1u;
    else
        return 0u;
    final = (unsigned char const *)memchr(input + prefix, 'q', body - prefix);
    if (final == NULL || (size_t)(final - input) >= length - 2u)
        return 0u;

    prefix = (size_t)(final - input) + 1u;
    cuts = sixel_decode_prescan_cuts(plan, length - 2u - prefix, &cut_count);
    if (cuts == NULL || cut_count >= capacity)
        return 0u;
    bias = (uint32_t)(body - prefix);
    count = 0u;
    band = 0u;
    seg = 0u;
    start = 0u;
    for (i = 0; i < cut_count; ++i) {
        offset = cuts[i] & SIXEL_PRESCAN_CUT_OFFSET;
        if (offset < bias)
            continue;
        offset -= bias;
        kind = cuts[i] >> SIXEL_PRESCAN_CUT_SHIFT;
        if (kind == SIXEL_PRESCAN_CUT_GRAPHIC && offset <= start)
            continue;
        decoder_gpu_add_chunk(segments, &count, start, offset, band, seg);
        start = offset + (kind != SIXEL_PRESCAN_CUT_GRAPHIC);
        band += kind == SIXEL_PRESCAN_CUT_NL;
        seg += kind != SIXEL_PRESCAN_CUT_GRAPHIC;
    }
    *end = (uint32_t)(length - 2u - body);
    *bands = band + 1u;
    decoder_gpu_add_chunk(segments, &count, start, *end, band, seg);
    return count;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_decoder_gpu_parse_columns(unsigned char const *input,
                                size_t length,
                                size_t body,
                                int width,
                                int height,
                                int initial_color,
                                int trust_raster_size,
                                uint64_t *columns,
                                int *alpha_opaque,
                                int *painted_outside_raster,
                                sixel_decode_prescan_t const *plan,
                                sixel_timeline_logger_t *logger)
{
    decoder_gpu_params_t params;
    id<MTLCommandBuffer> command;
    id<MTLBlitCommandEncoder> blit;
    id<MTLComputeCommandEncoder> compute;
    NSUInteger simd;
    size_t span;
    size_t words;
    uint32_t capacity;
    uint32_t count;
    uint32_t end;
    uint32_t reached;
    uint32_t flags;
    int prescanned;
    SIXELSTATUS status;

    if (input == NULL || columns == NULL || alpha_opaque == NULL ||
            painted_outside_raster == NULL || body >= length ||
            length - body > UINT32_MAX / 2u || width <= 0 || height <= 0 ||
            initial_color < 0 || initial_color >= 256)
        return SIXEL_FALSE;
    span = length - body;
    words = (size_t)width * (size_t)((height + 5) / 6);
    if (words > UINT32_MAX / 6u)
        return SIXEL_FALSE;
    if (pthread_mutex_trylock(&g_decoder_gpu_lock) != 0)
        return SIXEL_FALSE;
    status = SIXEL_FALSE;
    if (!decoder_gpu_prepare())
        goto end;
    @autoreleasepool {
        /* Chunk cuts are at least a block apart; a body dense with
         * delimiters overflows the capacity and takes the CPU parser. */
        capacity = (uint32_t)(span / 32u) + 1024u;
        if (capacity > DECODER_GPU_MAX_SEGMENTS)
            capacity = DECODER_GPU_MAX_SEGMENTS;
        if (!decoder_gpu_reserve(&g_decoder_gpu_body, span,
                    MTLResourceStorageModeShared |
                    MTLResourceCPUCacheModeWriteCombined) ||
                !decoder_gpu_reserve(&g_decoder_gpu_segments,
                    (NSUInteger)capacity * sizeof(decoder_gpu_segment_t),
                    MTLResourceStorageModeShared) ||
                !decoder_gpu_reserve(&g_decoder_gpu_pixels,
                    (NSUInteger)words * 6u * sizeof(uint32_t),
                    MTLResourceStorageModePrivate) ||
                !decoder_gpu_reserve(&g_decoder_gpu_columns,
                    (NSUInteger)words * sizeof(uint64_t),
                    MTLResourceStorageModeShared))
            goto end;
        count = 0u;
        if (plan != NULL && body <= length - 2u) {
            count = decoder_gpu_rebase(plan, input, length, body,
                (decoder_gpu_segment_t *)[g_decoder_gpu_segments contents],
                capacity, &end, &reached);
        }
        prescanned = count != 0u;
        if (!prescanned) {
            count = decoder_gpu_split(input + body, (uint32_t)span,
                (decoder_gpu_segment_t *)[g_decoder_gpu_segments contents],
                capacity, &end, &reached);
        }
        if (count == 0u ||
                !decoder_gpu_reserve(&g_decoder_gpu_info,
                    (NSUInteger)count * 2u * sizeof(uint32_t),
                    MTLResourceStorageModePrivate))
            goto end;
        memcpy([g_decoder_gpu_body contents], input + body, end);
        *(uint32_t *)[g_decoder_gpu_flags contents] = 0u;
        params.segments = count;
        params.width = (uint32_t)width;
        params.height = (uint32_t)height;
        params.bands = (uint32_t)((height + 5) / 6);
        params.initial_color = (uint32_t)initial_color;

        command = [g_decoder_gpu_queue commandBuffer];
        blit = [command blitCommandEncoder];
        if (blit == nil)
            goto end;
        [blit fillBuffer:g_decoder_gpu_pixels
                   range:NSMakeRange(0, words * 6u * sizeof(uint32_t))
                   value:0];
        [blit endEncoding];
        compute = [command computeCommandEncoder];
        if (compute == nil)
            goto end;
        simd = [g_decoder_gpu_measure threadExecutionWidth];
        [compute setComputePipelineState:g_decoder_gpu_measure];
        [compute setBuffer:g_decoder_gpu_body offset:0 atIndex:0];
        [compute setBuffer:g_decoder_gpu_segments offset:0 atIndex:1];
        [compute setBuffer:g_decoder_gpu_info offset:0 atIndex:2];
        [compute setBytes:&params length:sizeof(params) atIndex:3];
        [compute dispatchThreads:MTLSizeMake(count, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(simd * 2u, 1, 1)];
        simd = [g_decoder_gpu_parse threadExecutionWidth];
        [compute setComputePipelineState:g_decoder_gpu_parse];
        [compute setBuffer:g_decoder_gpu_body offset:0 atIndex:0];
        [compute setBuffer:g_decoder_gpu_segments offset:0 atIndex:1];
        [compute setBuffer:g_decoder_gpu_pixels offset:0 atIndex:2];
        [compute setBytes:&params length:sizeof(params) atIndex:3];
        [compute setBuffer:g_decoder_gpu_flags offset:0 atIndex:4];
        [compute setBuffer:g_decoder_gpu_info offset:0 atIndex:5];
        [compute dispatchThreads:MTLSizeMake(count, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(simd * 2u, 1, 1)];
        simd = [g_decoder_gpu_pack threadExecutionWidth];
        [compute setComputePipelineState:g_decoder_gpu_pack];
        [compute setBuffer:g_decoder_gpu_pixels offset:0 atIndex:0];
        [compute setBuffer:g_decoder_gpu_columns offset:0 atIndex:1];
        [compute setBytes:&params length:sizeof(params) atIndex:2];
        [compute setBuffer:g_decoder_gpu_flags offset:0 atIndex:3];
        [compute dispatchThreads:MTLSizeMake(words, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(simd * 8u, 1, 1)];
        [compute endEncoding];
        [command commit];
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        if ([command status] != MTLCommandBufferStatusCompleted)
            goto end;
        flags = *(uint32_t const *)[g_decoder_gpu_flags contents];
        if ((flags & DECODER_GPU_NEEDS_CPU) != 0u ||
                ((flags & DECODER_GPU_OUTSIDE) != 0u && !trust_raster_size))
            goto end;
        memcpy(columns, [g_decoder_gpu_columns contents],
               words * sizeof(uint64_t));
        *painted_outside_raster = (flags & DECODER_GPU_OUTSIDE) != 0u;
        /* Like the gather, bands the body never reached leave it unknown. */
        *alpha_opaque = reached < params.bands ? (-1)
            : (flags & DECODER_GPU_TRANSPARENT) == 0u;
        if (logger != NULL) {
            sixel_timeline_logger_logf(logger, "decoder", "metal",
                prescanned ? "complete_prescan" : "complete", 0);
        }
        status = SIXEL_OK;
    }
end:
    pthread_mutex_unlock(&g_decoder_gpu_lock);
    return status;
}
#endif
