/* SPDX-License-Identifier: MIT */
/* Metal backend for encoder-body-gpu.h. */

#include "config.h"

#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "encoder-body-gpu.h"
#include "gpu-runtime-metal.h"

static NSString *const sixel_body_gpu_source = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct body_params {\n"
"  uint width; uint height; uint encoded_width; uint encoded_height;\n"
"  uint offset_left; uint offset_top; uint ncolors; int keycolor;\n"
"  uint words; uint bands; uint has_mask; uint pad;\n"
"};\n"
/* Mark each painted source pixel in its (band, register) column bitmap. */
"kernel void body_classify(device const uchar *indexes [[buffer(0)]],\n"
"                          device const uchar *mask [[buffer(1)]],\n"
"                          device atomic_uint *occupancy [[buffer(2)]],\n"
"                          constant body_params &p [[buffer(3)]],\n"
"                          uint2 g [[thread_position_in_grid]]) {\n"
"  if (g.x >= p.width || g.y >= p.height) return;\n"
"  uint i = g.y * p.width + g.x;\n"
"  if (p.has_mask != 0 && mask[i] != 0) return;\n"
"  uint c = indexes[i];\n"
"  if (c >= p.ncolors || (int)c == p.keycolor) return;\n"
"  uint x = g.x + p.offset_left;\n"
"  uint band = (g.y + p.offset_top) / 6;\n"
"  atomic_fetch_or_explicit(&occupancy[(band * p.ncolors + c) * p.words + x / 32],\n"
"                           1u << (x % 32), memory_order_relaxed);\n"
"}\n"
"static uint body_next(device const uint *o, uint words, uint limit, uint from) {\n"
"  uint w = from / 32;\n"
"  if (w >= words) return limit;\n"
"  uint bits = o[w] & (0xffffffffu << (from % 32));\n"
"  while (bits == 0) { if (++w >= words) return limit; bits = o[w]; }\n"
"  return min(w * 32 + ctz(bits), limit);\n"
"}\n"
/* One thread per (band, register).  The count pass sizes the spans and their
 * column values; the write pass stores them at the prefix offsets, together
 * with the register's first row-major position in the band. */
"kernel void body_spans(device const uint *occupancy [[buffer(0)]],\n"
"                       device uint *counts [[buffer(1)]],\n"
"                       device uint *widths [[buffer(2)]],\n"
"                       device const uint *node_first [[buffer(3)]],\n"
"                       device const uint *map_first [[buffer(4)]],\n"
"                       device uint4 *nodes [[buffer(5)]],\n"
"                       device uchar *map [[buffer(6)]],\n"
"                       device const uchar *indexes [[buffer(7)]],\n"
"                       device const uchar *mask [[buffer(8)]],\n"
"                       constant body_params &p [[buffer(9)]],\n"
"                       constant uint &write_pass [[buffer(10)]],\n"
"                       device uint *first_seen [[buffer(11)]],\n"
"                       uint t [[thread_position_in_grid]]) {\n"
"  if (t >= p.bands * p.ncolors) return;\n"
"  device const uint *o = occupancy + t * p.words;\n"
"  uint band = t / p.ncolors; uint c = t % p.ncolors;\n"
"  uint limit = p.encoded_width;\n"
"  uint n = 0; uint used = 0; uint first = 0xffffffffu;\n"
"  uint node_base = write_pass != 0 ? node_first[t] : 0;\n"
"  uint map_base = write_pass != 0 ? map_first[t] : 0;\n"
"  uint sx = body_next(o, p.words, limit, 0);\n"
"  while (sx < limit) {\n"
"    uint mx = sx + 1;\n"
"    uint nx = body_next(o, p.words, limit, mx);\n"
"    while (nx < limit && nx - mx <= 9) { mx = nx + 1; nx = body_next(o, p.words, limit, mx); }\n"
"    if (write_pass != 0) {\n"
"      nodes[node_base + n] = uint4(c, sx, mx, map_base + used);\n"
"      for (uint x = sx; x < mx; x++) {\n"
"        uint v = 0;\n"
"        int sxp = (int)x - (int)p.offset_left;\n"
"        if (sxp >= 0 && (uint)sxp < p.width) {\n"
"          for (uint r = 0; r < 6; r++) {\n"
"            int syp = (int)(band * 6 + r) - (int)p.offset_top;\n"
"            if (syp < 0 || (uint)syp >= p.height) continue;\n"
"            uint i = (uint)syp * p.width + (uint)sxp;\n"
"            if (indexes[i] == c && (p.has_mask == 0 || mask[i] == 0)) v |= 1u << r;\n"
"          }\n"
"        }\n"
"        if (v != 0) first = min(first, ctz(v) * p.encoded_width + x);\n"
"        map[map_base + used + (x - sx)] = (uchar)v;\n"
"      }\n"
"    }\n"
"    used += mx - sx; n++; sx = nx;\n"
"  }\n"
"  if (write_pass == 0) { counts[t] = n; widths[t] = used; }\n"
"  else first_seen[t] = first;\n"
"}\n"
/* Emission. The spelling follows sixel_put_node: a run of at most three
 * cells repeats its symbol, a longer one is '!' count symbol, and the
 * limited form first peels '!255' symbol pieces from runs above 255. */
"struct emit_params {\n"
"  uint encoded_width; uint words; uint ncolors; uint bands;\n"
"  uint selectors; uint limited; uint nodes; uint group_limit;\n"
"};\n"
"static uint emit_digits(uint v) {\n"
"  uint n = 1; while (v >= 10) { v /= 10; n++; } return n;\n"
"}\n"
"static uint emit_run_length(uint c, uint limited) {\n"
"  uint n = 0;\n"
"  if (limited != 0 && c > 255) { uint k = (c - 1) / 255; n = k * 5; c -= k * 255; }\n"
"  return n + (c <= 3 ? c : 2 + emit_digits(c));\n"
"}\n"
"static uint emit_number(device uchar *out, uint o, uint v) {\n"
"  uint n = emit_digits(v);\n"
"  for (uint i = n; i > 0; i--) { out[o + i - 1] = (uchar)('0' + v % 10); v /= 10; }\n"
"  return o + n;\n"
"}\n"
"static uint emit_run(device uchar *out, uint o, uchar s, uint c, uint limited) {\n"
"  if (limited != 0 && c > 255) {\n"
"    uint k = (c - 1) / 255; c -= k * 255;\n"
"    for (; k > 0; k--) { out[o] = '!'; out[o + 1] = '2'; out[o + 2] = '5'; out[o + 3] = '5'; out[o + 4] = s; o += 5; }\n"
"  }\n"
"  if (c <= 3) { for (; c > 0; c--) out[o++] = s; return o; }\n"
"  out[o++] = '!'; o = emit_number(out, o, c); out[o++] = s;\n"
"  return o;\n"
"}\n"
/* One thread per span: the spelled length of its column values. */
"kernel void body_lengths(device const uint4 *nodes [[buffer(0)]],\n"
"                         device const uchar *map [[buffer(1)]],\n"
"                         device uint *lengths [[buffer(2)]],\n"
"                         constant emit_params &p [[buffer(3)]],\n"
"                         uint t [[thread_position_in_grid]]) {\n"
"  if (t >= p.nodes) return;\n"
"  uint4 n = nodes[t];\n"
"  device const uchar *row = map + n.w;\n"
"  uint w = n.z - n.y; uint total = 0;\n"
"  for (uint i = 0; i < w;) {\n"
"    uchar v = row[i]; uint e = i + 1;\n"
"    while (e < w && row[e] == v) e++;\n"
"    total += emit_run_length(e - i, p.limited); i = e;\n"
"  }\n"
"  lengths[t] = total;\n"
"}\n"
/* One thread per band reproduces sixel_band_emit. Each column holds its
 * spans by farther end, then later first-seen register; a sweep takes the
 * first remaining start at or after the cursor, and a new sweep begins
 * with '$' at the first remaining start. Each emitted span records its
 * node, its byte offset in the band, the cursor before it, and flags. */
"kernel void body_order(device const uint4 *nodes [[buffer(0)]],\n"
"                       device const uint *band_first [[buffer(1)]],\n"
"                       device const uint *first_seen [[buffer(2)]],\n"
"                       device const uint *lengths [[buffer(3)]],\n"
"                       device const uint *registers [[buffer(4)]],\n"
"                       device uint *columns [[buffer(5)]],\n"
"                       device uint *bits [[buffer(6)]],\n"
"                       device uint *sorted [[buffer(7)]],\n"
"                       device uint4 *entries [[buffer(8)]],\n"
"                       device uint *band_size [[buffer(9)]],\n"
"                       constant emit_params &p [[buffer(10)]],\n"
"                       uint b [[thread_position_in_grid]]) {\n"
"  if (b >= p.bands) return;\n"
"  uint first = band_first[b]; uint end = band_first[b + 1];\n"
"  if (end - first <= p.group_limit) return;\n"
"  uint width = p.encoded_width;\n"
"  device uint *head = columns + (ulong)b * (2 * width + 1);\n"
"  device uint *stop = head + width + 1;\n"
"  device uint *o = bits + (ulong)b * p.words;\n"
"  device const uint *rank = first_seen + (ulong)b * p.ncolors;\n"
"  for (uint x = 0; x <= width; x++) head[x] = 0;\n"
"  for (uint w = 0; w < p.words; w++) o[w] = 0;\n"
"  for (uint i = first; i < end; i++) head[nodes[i].y + 1]++;\n"
"  for (uint x = 0; x < width; x++) head[x + 1] += head[x];\n"
"  for (uint x = 0; x < width; x++) stop[x] = head[x];\n"
"  for (uint i = first; i < end; i++) {\n"
"    uint4 n = nodes[i]; uint k = first + stop[n.y]++;\n"
"    uint j = k;\n"
"    while (j > first + head[n.y]) {\n"
"      uint4 q = nodes[sorted[j - 1]];\n"
"      if (q.z > n.z || (q.z == n.z && rank[q.x] > rank[n.x])) break;\n"
"      sorted[j] = sorted[j - 1]; j--;\n"
"    }\n"
"    sorted[j] = i;\n"
"    o[n.y / 32] |= 1u << (n.y % 32);\n"
"  }\n"
"  uint pos = b > 0 ? 1 : 0; uint k = first; uint x = 0; uint active = 0xffffffffu;\n"
"  uint sx = body_next(o, p.words, width, 0);\n"
"  while (sx < width) {\n"
"    uint dollar = 0;\n"
"    if (x > sx) { dollar = 1; x = 0; }\n"
"    do {\n"
"      uint i = sorted[first + head[sx]];\n"
"      uint4 n = nodes[i];\n"
"      uint sel = p.selectors != 0 && active != n.x ? 1 : 0;\n"
"      entries[k++] = uint4(i, pos, x, dollar | sel << 1 | b << 2);\n"
"      pos += dollar + (sel != 0 ? 1 + emit_digits(registers[n.x]) : 0)\n"
"        + (x < sx ? emit_run_length(sx - x, p.limited) : 0) + lengths[i];\n"
"      if (sel != 0) active = n.x;\n"
"      dollar = 0; x = n.z;\n"
"      if (++head[sx] == stop[sx]) o[sx / 32] &= ~(1u << (sx % 32));\n"
"      sx = body_next(o, p.words, width, x);\n"
"    } while (sx < width);\n"
"    sx = body_next(o, p.words, width, 0);\n"
"  }\n"
"  band_size[b] = pos;\n"
"}\n"
/* Bands of at most BODY_GROUP_NODES spans take one threadgroup each. The
 * threads sort the spans by start, farther end, then later first-seen
 * register, which is the column order of body_order; one thread then takes
 * the sweeps from threadgroup memory, and the threads sum the spelled
 * lengths into offsets. The first remaining span at or after the cursor is
 * the first unconsumed sorted span whose start reaches it, since each
 * column's spans are consumed from its head. */
"#define BODY_GROUP_NODES 4096\n"
"#define BODY_GROUP_THREADS 256\n"
"#define BODY_GROUP_COLUMNS 2048\n"
"kernel void body_order_group(device const uint4 *nodes [[buffer(0)]],\n"
"                             device const uint *band_first [[buffer(1)]],\n"
"                             device const uint *first_seen [[buffer(2)]],\n"
"                             device const uint *lengths [[buffer(3)]],\n"
"                             device const uint *registers [[buffer(4)]],\n"
"                             device ulong *keys [[buffer(5)]],\n"
"                             device ushort *slots [[buffer(6)]],\n"
"                             device uint4 *entries [[buffer(7)]],\n"
"                             device uint *band_size [[buffer(8)]],\n"
"                             constant emit_params &p [[buffer(9)]],\n"
"                             uint b [[threadgroup_position_in_grid]],\n"
"                             uint t [[thread_position_in_threadgroup]]) {\n"
"  threadgroup uint spans[BODY_GROUP_NODES];\n"
"  threadgroup uchar colors[BODY_GROUP_NODES];\n"
"  threadgroup uint consumed[BODY_GROUP_NODES / 32];\n"
"  threadgroup uint partial[BODY_GROUP_THREADS];\n"
"  threadgroup ushort column[BODY_GROUP_COLUMNS + 1];\n"
"  uint first = band_first[b]; uint n = band_first[b + 1] - first;\n"
"  if (n > p.group_limit) return;\n"
"  uint size = 1; while (size < n) size <<= 1;\n"
"  device ulong *k = keys + (ulong)b * BODY_GROUP_NODES;\n"
"  device ushort *s = slots + (ulong)b * BODY_GROUP_NODES;\n"
"  device const uint *rank = first_seen + (ulong)b * p.ncolors;\n"
"  for (uint i = t; i < size; i += BODY_GROUP_THREADS) {\n"
"    if (i < n) {\n"
"      uint4 q = nodes[first + i];\n"
"      k[i] = (ulong)q.y << 48 | (ulong)(0xffffu - q.z) << 32 | (ulong)(0xffffffffu - rank[q.x]);\n"
"    } else k[i] = ~0ul;\n"
"    s[i] = (ushort)i;\n"
"  }\n"
"  for (uint i = t; i < BODY_GROUP_NODES / 32; i += BODY_GROUP_THREADS) consumed[i] = 0;\n"
"  threadgroup_barrier(mem_flags::mem_device);\n"
"  for (uint w = 2; w <= size; w <<= 1) {\n"
"    for (uint j = w >> 1; j > 0; j >>= 1) {\n"
"      for (uint i = t; i < size; i += BODY_GROUP_THREADS) {\n"
"        uint l = i ^ j;\n"
"        if (l > i) {\n"
"          ulong a = k[i]; ulong c = k[l];\n"
"          bool up = (i & w) == 0;\n"
"          if (up ? a > c : a < c) {\n"
"            k[i] = c; k[l] = a; ushort v = s[i]; s[i] = s[l]; s[l] = v;\n"
"          }\n"
"        }\n"
"      }\n"
"      threadgroup_barrier(mem_flags::mem_device);\n"
"    }\n"
"  }\n"
"  for (uint i = t; i < n; i += BODY_GROUP_THREADS) {\n"
"    uint4 q = nodes[first + s[i]];\n"
"    spans[i] = q.y << 16 | q.z; colors[i] = (uchar)q.x;\n"
"  }\n"
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
/* With the frame no wider than the table, column[x] is the first sorted
 * position whose start reaches x; wider frames search for it. */
"  bool table = p.encoded_width <= BODY_GROUP_COLUMNS;\n"
"  for (uint i = t; table && i <= n; i += BODY_GROUP_THREADS) {\n"
"    uint lo = i > 0 ? (spans[i - 1] >> 16) + 1 : 0;\n"
"    uint hi = i < n ? spans[i] >> 16 : p.encoded_width;\n"
"    for (uint x = lo; x <= hi; x++) column[x] = (ushort)i;\n"
"  }\n"
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"  if (t == 0) {\n"
"    uint x = 0; uint e = 0; uint low = 0; uint active = 0xffffffffu;\n"
"    while (true) {\n"
"      while (low < n && (consumed[low / 32] >> (low % 32) & 1) != 0) low++;\n"
"      if (low >= n) break;\n"
"      uint at = low; uint dollar = 0;\n"
"      if (x > (spans[at] >> 16)) { dollar = 1; x = 0; }\n"
"      while (true) {\n"
"        uint sx = spans[at] >> 16; uint mx = spans[at] & 0xffffu;\n"
"        uint c = colors[at];\n"
"        uint sel = p.selectors != 0 && active != c ? 1 : 0;\n"
"        entries[first + e++] = uint4(first + s[at], 0, x, dollar | sel << 1 | b << 2);\n"
"        if (sel != 0) active = c;\n"
"        consumed[at / 32] |= 1u << (at % 32);\n"
"        dollar = 0; x = mx;\n"
"        uint lo = at + 1;\n"
"        if (table) lo = column[x];\n"
"        else { uint hi = n; while (lo < hi) { uint m = (lo + hi) / 2; if ((spans[m] >> 16) < x) lo = m + 1; else hi = m; } }\n"
"        uint w = lo / 32; uint bits = lo < n ? ~consumed[w] & (0xffffffffu << (lo % 32)) : 0;\n"
"        while (bits == 0 && ++w * 32 < n) bits = ~consumed[w];\n"
"        if (bits == 0) break;\n"
"        at = w * 32 + ctz(bits);\n"
"        if (at >= n) break;\n"
"      }\n"
"    }\n"
"  }\n"
"  threadgroup_barrier(mem_flags::mem_device);\n"
"  uint per = (n + BODY_GROUP_THREADS - 1) / BODY_GROUP_THREADS;\n"
"  uint lo = t * per; uint hi = min(n, lo + per); uint sum = 0;\n"
"  for (uint i = lo; i < hi; i++) {\n"
"    uint4 e = entries[first + i]; uint4 q = nodes[e.x];\n"
"    uint len = (e.w & 1) + ((e.w & 2) != 0 ? 1 + emit_digits(registers[q.x]) : 0)\n"
"      + (e.z < q.y ? emit_run_length(q.y - e.z, p.limited) : 0) + lengths[e.x];\n"
"    spans[i] = len; sum += len;\n"
"  }\n"
"  partial[t] = sum;\n"
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"  if (t == 0) {\n"
"    uint run = b > 0 ? 1 : 0;\n"
"    for (uint i = 0; i < BODY_GROUP_THREADS; i++) { uint v = partial[i]; partial[i] = run; run += v; }\n"
"    band_size[b] = run;\n"
"  }\n"
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"  uint pos = partial[t];\n"
"  for (uint i = lo; i < hi; i++) { entries[first + i].y = pos; pos += spans[i]; }\n"
"}\n"
/* Band offsets and the '-' that opens every band after the first. */
"kernel void body_bases(device const uint *band_size [[buffer(0)]],\n"
"                       device uint *band_bytes [[buffer(1)]],\n"
"                       device uchar *out [[buffer(2)]],\n"
"                       constant emit_params &p [[buffer(3)]],\n"
"                       uint t [[thread_position_in_grid]]) {\n"
"  if (t != 0) return;\n"
"  uint total = 0;\n"
"  for (uint b = 0; b < p.bands; b++) {\n"
"    band_bytes[b] = total;\n"
"    if (b > 0) out[total] = '-';\n"
"    total += band_size[b];\n"
"  }\n"
"  band_bytes[p.bands] = total;\n"
"}\n"
"kernel void body_write(device const uint4 *nodes [[buffer(0)]],\n"
"                       device const uchar *map [[buffer(1)]],\n"
"                       device const uint4 *entries [[buffer(2)]],\n"
"                       device const uint *band_bytes [[buffer(3)]],\n"
"                       device const uint *registers [[buffer(4)]],\n"
"                       device uchar *out [[buffer(5)]],\n"
"                       constant emit_params &p [[buffer(6)]],\n"
"                       uint t [[thread_position_in_grid]]) {\n"
"  if (t >= p.nodes) return;\n"
"  uint4 e = entries[t];\n"
"  uint4 n = nodes[e.x];\n"
"  uint o = band_bytes[e.w >> 2] + e.y;\n"
"  if ((e.w & 1) != 0) out[o++] = '$';\n"
"  if ((e.w & 2) != 0) { out[o++] = '#'; o = emit_number(out, o, registers[n.x]); }\n"
"  if (e.z < n.y) o = emit_run(out, o, '?', n.y - e.z, p.limited);\n"
"  device const uchar *row = map + n.w;\n"
"  uint w = n.z - n.y;\n"
"  for (uint i = 0; i < w;) {\n"
"    uchar v = row[i]; uint f = i + 1;\n"
"    while (f < w && row[f] == v) f++;\n"
"    o = emit_run(out, o, (uchar)('?' + v), f - i, p.limited); i = f;\n"
"  }\n"
"}\n";

typedef struct sixel_body_gpu_params {
    uint32_t width, height, encoded_width, encoded_height;
    uint32_t offset_left, offset_top, ncolors;
    int32_t keycolor;
    uint32_t words, bands, has_mask, pad;
} sixel_body_gpu_params_t;

typedef struct sixel_body_gpu_emit_params {
    uint32_t encoded_width, words, ncolors, bands;
    uint32_t selectors, limited, nodes, group_limit;
} sixel_body_gpu_emit_params_t;

/* Mirrors BODY_GROUP_NODES and BODY_GROUP_THREADS in the shader. */
#define SIXEL_BODY_GROUP_NODES 4096U
#define SIXEL_BODY_GROUP_THREADS 256U

enum {
    BODY_INDEXES, BODY_MASK, BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS,
    BODY_NODE_FIRST, BODY_MAP_FIRST, BODY_NODES, BODY_MAP, BODY_FIRST_SEEN,
    BODY_BAND_FIRST, BODY_LENGTHS, BODY_REGISTERS, BODY_COLUMNS, BODY_BITS,
    BODY_SORTED, BODY_ENTRIES, BODY_BAND_SIZE, BODY_BAND_BYTES, BODY_BYTES,
    BODY_KEYS, BODY_KEY_SLOTS, BODY_SLOTS
};

enum {
    BODY_CLASSIFY, BODY_SPANS, BODY_LENGTH, BODY_ORDER, BODY_ORDER_GROUP,
    BODY_BASES, BODY_WRITE, BODY_PIPELINES
};

static char const *const sixel_body_gpu_kernels[BODY_PIPELINES] = {
    "body_classify", "body_spans", "body_lengths", "body_order",
    "body_order_group", "body_bases", "body_write"
};

struct sixel_body_gpu {
    void *device;           /* id<MTLDevice>, retained */
    void *queue;            /* id<MTLCommandQueue> */
    void *pipelines[BODY_PIPELINES];    /* id<MTLComputePipelineState> */
    void *buffers[BODY_SLOTS];  /* id<MTLBuffer>, grown on demand */
    size_t lengths[BODY_SLOTS];
    uint32_t *band_first;
    size_t band_first_count;
};

static id<MTLBuffer>
sixel_body_gpu_buffer(sixel_body_gpu_t *context, int slot, size_t length)
{
    id<MTLBuffer> buffer;

    if (length == 0) {
        length = 16;
    }
    if (context->buffers[slot] != NULL && context->lengths[slot] >= length) {
        return (__bridge id<MTLBuffer>)context->buffers[slot];
    }
    if (context->buffers[slot] != NULL) {
        CFRelease(context->buffers[slot]);
        context->buffers[slot] = NULL;
        context->lengths[slot] = 0;
    }
    /* Grow by half again so slowly growing frames do not reallocate each
     * time. Shared storage lets the CPU read the result in place. */
    length += length / 2;
    buffer = [(__bridge id<MTLDevice>)context->device
        newBufferWithLength:length options:MTLResourceStorageModeShared];
    if (buffer == nil) {
        return nil;
    }
    context->buffers[slot] = (void *)CFBridgingRetain(buffer);
    context->lengths[slot] = length;
    return buffer;
}

SIXELSTATUS
sixel_body_gpu_new(sixel_body_gpu_t **result)
{
    sixel_body_gpu_t *context;
    SIXELSTATUS status;

    *result = NULL;
    context = (sixel_body_gpu_t *)calloc(1, sizeof(*context));
    if (context == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = SIXEL_FEATURE_ERROR;
    @autoreleasepool {
        id<MTLDevice> device;
        id<MTLLibrary> library;
        id<MTLComputePipelineState> pipeline;
        id<MTLCommandQueue> queue;
        NSError *error;
        int i;

        error = nil;
        device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            goto end;
        }
        library = [device newLibraryWithSource:sixel_body_gpu_source
                                       options:nil
                                         error:&error];
        if (library == nil) {
            goto end;
        }
        context->device = (void *)CFBridgingRetain(device);
        for (i = 0; i < BODY_PIPELINES; ++i) {
            pipeline = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:
                    [NSString stringWithUTF8String:
                        sixel_body_gpu_kernels[i]]] error:&error];
            if (pipeline == nil) {
                goto end;
            }
            context->pipelines[i] = (void *)CFBridgingRetain(pipeline);
        }
        queue = [device newCommandQueue];
        if (queue == nil) {
            goto end;
        }
        context->queue = (void *)CFBridgingRetain(queue);
        status = SIXEL_OK;
    }
end:
    if (SIXEL_FAILED(status)) {
        sixel_body_gpu_destroy(context);
        return status;
    }
    *result = context;
    return SIXEL_OK;
}

void
sixel_body_gpu_destroy(sixel_body_gpu_t *context)
{
    size_t i;

    if (context == NULL) {
        return;
    }
    for (i = 0; i < sizeof(context->buffers) / sizeof(context->buffers[0]);
            ++i) {
        if (context->buffers[i] != NULL) {
            CFRelease(context->buffers[i]);
        }
    }
    for (i = 0; i < BODY_PIPELINES; ++i) {
        if (context->pipelines[i] != NULL) CFRelease(context->pipelines[i]);
    }
    if (context->queue != NULL) CFRelease(context->queue);
    if (context->device != NULL) CFRelease(context->device);
    free(context->band_first);
    free(context);
}

static void
sixel_body_gpu_encode_spans(id<MTLComputeCommandEncoder> encoder,
                            sixel_body_gpu_t *context,
                            sixel_body_gpu_params_t const *params,
                            uint32_t write_pass,
                            id<MTLBuffer> indexes, id<MTLBuffer> mask)
{
    static int const slots[] = {
        BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS, BODY_NODE_FIRST,
        BODY_MAP_FIRST, BODY_NODES, BODY_MAP
    };
    NSUInteger i;
    NSUInteger threads;
    NSUInteger group;
    id<MTLComputePipelineState> pipeline;

    pipeline = (__bridge id<MTLComputePipelineState>)
        context->pipelines[BODY_SPANS];
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
    [encoder setBuffer:indexes offset:0 atIndex:7];
    [encoder setBuffer:mask offset:0 atIndex:8];
    [encoder setBytes:params length:sizeof(*params) atIndex:9];
    [encoder setBytes:&write_pass length:sizeof(write_pass) atIndex:10];
    [encoder setBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_FIRST_SEEN] offset:0 atIndex:11];
    threads = (NSUInteger)params->bands * params->ncolors;
    group = [pipeline maxTotalThreadsPerThreadgroup];
    if (group > 256) group = 256;
    [encoder dispatchThreads:MTLSizeMake(threads, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
}

static void
sixel_body_gpu_dispatch(id<MTLComputeCommandEncoder> encoder,
                        sixel_body_gpu_t *context, int kernel,
                        int const *slots, NSUInteger count,
                        sixel_body_gpu_emit_params_t const *params,
                        NSUInteger threads)
{
    id<MTLComputePipelineState> pipeline;
    NSUInteger group;
    NSUInteger i;

    pipeline = (__bridge id<MTLComputePipelineState>)
        context->pipelines[kernel];
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < count; ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
    [encoder setBytes:params length:sizeof(*params) atIndex:count];
    group = [pipeline maxTotalThreadsPerThreadgroup];
    if (group > 64) group = 64;
    [encoder dispatchThreads:MTLSizeMake(threads > 0 ? threads : 1, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
}

/* Spell the body after the write pass: span lengths, the per-band order
 * and offsets, band bases, then the bytes. */
static void
sixel_body_gpu_encode_emit(id<MTLComputeCommandEncoder> encoder,
                           sixel_body_gpu_t *context,
                           sixel_body_gpu_emit_params_t const *params)
{
    static int const lengths[] = { BODY_NODES, BODY_MAP, BODY_LENGTHS };
    static int const order[] = {
        BODY_NODES, BODY_BAND_FIRST, BODY_FIRST_SEEN, BODY_LENGTHS,
        BODY_REGISTERS, BODY_COLUMNS, BODY_BITS, BODY_SORTED, BODY_ENTRIES,
        BODY_BAND_SIZE
    };
    static int const group[] = {
        BODY_NODES, BODY_BAND_FIRST, BODY_FIRST_SEEN, BODY_LENGTHS,
        BODY_REGISTERS, BODY_KEYS, BODY_KEY_SLOTS, BODY_ENTRIES,
        BODY_BAND_SIZE
    };
    static int const bases[] = { BODY_BAND_SIZE, BODY_BAND_BYTES, BODY_BYTES };
    id<MTLComputePipelineState> pipeline;
    NSUInteger i;
    static int const write[] = {
        BODY_NODES, BODY_MAP, BODY_ENTRIES, BODY_BAND_BYTES, BODY_REGISTERS,
        BODY_BYTES
    };

    sixel_body_gpu_dispatch(encoder, context, BODY_LENGTH, lengths,
        sizeof(lengths) / sizeof(lengths[0]), params, params->nodes);
    sixel_body_gpu_dispatch(encoder, context, BODY_ORDER, order,
        sizeof(order) / sizeof(order[0]), params, params->bands);
    pipeline = (__bridge id<MTLComputePipelineState>)
        context->pipelines[BODY_ORDER_GROUP];
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < sizeof(group) / sizeof(group[0]); ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[group[i]]
                    offset:0 atIndex:i];
    }
    [encoder setBytes:params length:sizeof(*params)
              atIndex:sizeof(group) / sizeof(group[0])];
    [encoder dispatchThreadgroups:MTLSizeMake(params->bands, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(SIXEL_BODY_GROUP_THREADS, 1, 1)];
    sixel_body_gpu_dispatch(encoder, context, BODY_BASES, bases,
        sizeof(bases) / sizeof(bases[0]), params, 1);
    sixel_body_gpu_dispatch(encoder, context, BODY_WRITE, write,
        sizeof(write) / sizeof(write[0]), params, params->nodes);
}

SIXELSTATUS
sixel_body_gpu_run(sixel_body_gpu_t *context,
                   sixel_body_gpu_request_t const *request,
                   sixel_body_gpu_result_t *result)
{
    sixel_body_gpu_params_t params;
    size_t pixels;
    size_t pairs;
    size_t occupancy_words;
    size_t t;
    size_t b;
    uint64_t nodes_total;
    uint64_t map_total;
    uint32_t *counts;
    uint32_t *widths;
    uint32_t *node_first;
    uint32_t *map_first;
    sixel_body_gpu_emit_params_t emit;
    size_t gap_bound;
    size_t byte_bound;
    int spell;

    if (context == NULL || request == NULL || result == NULL ||
            request->indexes == NULL || request->width <= 0 ||
            request->height <= 0 || request->ncolors <= 0 ||
            request->ncolors > 256 || request->offset_left < 0 ||
            request->offset_top < 0 ||
            request->encoded_width < request->width + request->offset_left ||
            request->encoded_height <
                request->height + request->offset_top) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(&params, 0, sizeof(params));
    params.width = (uint32_t)request->width;
    params.height = (uint32_t)request->height;
    params.encoded_width = (uint32_t)request->encoded_width;
    params.encoded_height = (uint32_t)request->encoded_height;
    params.offset_left = (uint32_t)request->offset_left;
    params.offset_top = (uint32_t)request->offset_top;
    params.ncolors = (uint32_t)request->ncolors;
    params.keycolor = request->keycolor;
    params.words = (params.encoded_width + 31U) / 32U;
    params.bands = (params.encoded_height + 5U) / 6U;
    params.has_mask = request->mask != NULL;
    pixels = (size_t)request->width * (size_t)request->height;
    pairs = (size_t)params.bands * params.ncolors;
    occupancy_words = pairs * params.words;
    if (pairs > UINT32_MAX || occupancy_words > SIZE_MAX / 4U) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    @autoreleasepool {
        id<MTLCommandQueue> queue;
        id<MTLCommandBuffer> command;
        id<MTLBlitCommandEncoder> blit;
        id<MTLComputeCommandEncoder> encoder;
        id<MTLComputePipelineState> classify;
        id<MTLBuffer> indexes;
        id<MTLBuffer> mask;
        int staged;

        /* Bind shared frame storage when it lives on this context's
         * device; otherwise stage both planes into owned buffers. */
        indexes = nil;
        mask = nil;
        if (request->device != NULL &&
                sixel_gpu_metal_device(request->device) ==
                    (__bridge id<MTLDevice>)context->device) {
            indexes = sixel_gpu_metal_buffer(request->device,
                request->indexes_buffer);
            mask = request->mask != NULL
                ? sixel_gpu_metal_buffer(request->device,
                    request->mask_buffer) : nil;
            if (indexes == nil || [indexes length] < pixels ||
                    [indexes contents] != (void *)request->indexes ||
                    (request->mask != NULL && (mask == nil ||
                        [mask length] < pixels ||
                        [mask contents] != (void *)request->mask))) {
                indexes = nil;
                mask = nil;
            }
        }
        staged = indexes == nil;
        if (staged) {
            indexes = sixel_body_gpu_buffer(context, BODY_INDEXES, pixels);
        }
        if (mask == nil) {
            mask = sixel_body_gpu_buffer(context, BODY_MASK,
                staged && request->mask != NULL ? pixels : 16);
        }
        if (indexes == nil || mask == nil ||
                sixel_body_gpu_buffer(context, BODY_OCCUPANCY,
                    occupancy_words * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_COUNTS, pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_FIRST_SEEN,
                    pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_WIDTHS, pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_NODE_FIRST,
                    pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP_FIRST,
                    pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_NODES, 16) == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP, 16) == nil) {
            return SIXEL_BAD_ALLOCATION;
        }
        if (staged) {
            memcpy([indexes contents], request->indexes, pixels);
            if (request->mask != NULL) {
                memcpy([mask contents], request->mask, pixels);
            }
        }

        queue = (__bridge id<MTLCommandQueue>)context->queue;
        classify = (__bridge id<MTLComputePipelineState>)
            context->pipelines[BODY_CLASSIFY];
        command = [queue commandBuffer];
        blit = [command blitCommandEncoder];
        [blit fillBuffer:(__bridge id<MTLBuffer>)
                context->buffers[BODY_OCCUPANCY]
                   range:NSMakeRange(0, occupancy_words * 4U) value:0];
        [blit endEncoding];
        encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:classify];
        [encoder setBuffer:indexes offset:0 atIndex:0];
        [encoder setBuffer:mask offset:0 atIndex:1];
        [encoder setBuffer:(__bridge id<MTLBuffer>)
            context->buffers[BODY_OCCUPANCY] offset:0 atIndex:2];
        [encoder setBytes:&params length:sizeof(params) atIndex:3];
        [encoder dispatchThreads:MTLSizeMake(params.width, params.height, 1)
           threadsPerThreadgroup:MTLSizeMake(32, 8, 1)];
        sixel_body_gpu_encode_spans(encoder, context, &params, 0U,
            indexes, mask);
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        if ([command status] != MTLCommandBufferStatusCompleted) {
            return SIXEL_RUNTIME_ERROR;
        }

        /* Prefix offsets; pairs are band-major, so each band's nodes are
         * contiguous and register-ordered. */
        if (context->band_first_count < (size_t)params.bands + 1U) {
            free(context->band_first);
            context->band_first = (uint32_t *)malloc(
                ((size_t)params.bands + 1U) * sizeof(uint32_t));
            if (context->band_first == NULL) {
                context->band_first_count = 0;
                return SIXEL_BAD_ALLOCATION;
            }
            context->band_first_count = (size_t)params.bands + 1U;
        }
        counts = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_COUNTS] contents];
        widths = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_WIDTHS] contents];
        node_first = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_NODE_FIRST] contents];
        map_first = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_MAP_FIRST] contents];
        nodes_total = 0;
        map_total = 0;
        for (b = 0; b < params.bands; ++b) {
            context->band_first[b] = (uint32_t)nodes_total;
            for (t = b * params.ncolors; t < (b + 1U) * params.ncolors; ++t) {
                node_first[t] = (uint32_t)nodes_total;
                map_first[t] = (uint32_t)map_total;
                nodes_total += counts[t];
                map_total += widths[t];
            }
            if (nodes_total > UINT32_MAX || map_total > UINT32_MAX) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
        }
        context->band_first[params.bands] = (uint32_t)nodes_total;
        if (sixel_body_gpu_buffer(context, BODY_NODES,
                    (size_t)nodes_total * sizeof(sixel_body_gpu_node_t))
                    == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP,
                    (size_t)map_total + SIXEL_BODY_MAP_TAIL) == nil) {
            return SIXEL_BAD_ALLOCATION;
        }

        /* Every span costs at most '$', a selector, its leading gap and
         * one byte per column; a gap is no longer than the limited
         * spelling of the full width. */
        spell = request->registers != NULL;
        if (spell) {
            gap_bound = 5U * (params.encoded_width / 255U) + 2U;
            for (t = params.encoded_width; t > 0; t /= 10) ++gap_bound;
            byte_bound = (size_t)params.bands + (size_t)map_total +
                (size_t)nodes_total * (1U + 4U + gap_bound);
            if (byte_bound > UINT32_MAX) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            memset(&emit, 0, sizeof(emit));
            emit.encoded_width = params.encoded_width;
            emit.words = params.words;
            emit.ncolors = params.ncolors;
            emit.bands = params.bands;
            emit.selectors = request->selectors != 0;
            emit.limited = request->limited != 0;
            emit.nodes = (uint32_t)nodes_total;
            /* Spans pack start and end in 16 bits each. */
            emit.group_limit = params.encoded_width <= 0xffffU
                ? SIXEL_BODY_GROUP_NODES : 0U;
            if (sixel_body_gpu_buffer(context, BODY_BAND_FIRST,
                        ((size_t)params.bands + 1U) * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_LENGTHS,
                        (size_t)nodes_total * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_REGISTERS,
                        (size_t)params.ncolors * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_COLUMNS,
                        (size_t)params.bands *
                        (2U * (size_t)params.encoded_width + 1U) * 4U)
                        == nil ||
                    sixel_body_gpu_buffer(context, BODY_BITS,
                        (size_t)params.bands * params.words * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_SORTED,
                        (size_t)nodes_total * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_ENTRIES,
                        (size_t)nodes_total * 16U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_BAND_SIZE,
                        (size_t)params.bands * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_BAND_BYTES,
                        ((size_t)params.bands + 1U) * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_BYTES,
                        byte_bound) == nil ||
                    sixel_body_gpu_buffer(context, BODY_KEYS,
                        (size_t)params.bands * SIXEL_BODY_GROUP_NODES * 8U)
                        == nil ||
                    sixel_body_gpu_buffer(context, BODY_KEY_SLOTS,
                        (size_t)params.bands * SIXEL_BODY_GROUP_NODES * 2U)
                        == nil) {
                return SIXEL_BAD_ALLOCATION;
            }
            memcpy([(__bridge id<MTLBuffer>)
                    context->buffers[BODY_BAND_FIRST] contents],
                context->band_first, ((size_t)params.bands + 1U) * 4U);
            memcpy([(__bridge id<MTLBuffer>)
                    context->buffers[BODY_REGISTERS] contents],
                request->registers, (size_t)params.ncolors * 4U);
        }

        command = [queue commandBuffer];
        encoder = [command computeCommandEncoder];
        sixel_body_gpu_encode_spans(encoder, context, &params, 1U,
            indexes, mask);
        if (spell) {
            sixel_body_gpu_encode_emit(encoder, context, &emit);
        }
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        if ([command status] != MTLCommandBufferStatusCompleted) {
            return SIXEL_RUNTIME_ERROR;
        }
        result->nodes = (sixel_body_gpu_node_t const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_NODES] contents];
        result->map = (unsigned char *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_MAP] contents];
        result->band_first = context->band_first;
        result->first_seen = (uint32_t const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_FIRST_SEEN] contents];
        result->ncolors = (int)params.ncolors;
        result->band_count = (int)params.bands;
        result->bytes = spell ? (unsigned char const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_BYTES] contents] : NULL;
        result->band_bytes = spell ? (uint32_t const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_BAND_BYTES] contents] : NULL;
    }
    return SIXEL_OK;
}
#endif /* HAVE_METAL */
