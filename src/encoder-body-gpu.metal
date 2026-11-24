/* SPDX-License-Identifier: MIT */
#include <metal_stdlib>
using namespace metal;
struct body_params {
  uint width; uint height; uint encoded_width; uint encoded_height;
  uint offset_left; uint offset_top; uint ncolors; int keycolor;
  uint words; uint bands; uint has_mask; uint nodes;
};
/* Mark each painted source pixel in its (band, register) column bitmap. */
/* Accel: BODY-COMPOSE metal */
kernel void body_classify(device const uchar *indexes [[buffer(0)]],
                          device const uchar *mask [[buffer(1)]],
                          device atomic_uint *occupancy [[buffer(2)]],
                          constant body_params &p [[buffer(3)]],
                          uint2 g [[thread_position_in_grid]]) {
  if (g.x >= p.width || g.y >= p.height) return;
  uint i = g.y * p.width + g.x;
  if (p.has_mask != 0 && mask[i] != 0) return;
  uint c = indexes[i];
  if (c >= p.ncolors || (int)c == p.keycolor) return;
  uint x = g.x + p.offset_left;
  uint band = (g.y + p.offset_top) / 6;
  atomic_fetch_or_explicit(&occupancy[(band * p.ncolors + c) * p.words + x / 32],
                           1u << (x % 32), memory_order_relaxed);
}
static uint body_next(device const uint *o, uint words, uint limit, uint from) {
  uint w = from / 32;
  if (w >= words) return limit;
  uint bits = o[w] & (0xffffffffu << (from % 32));
  while (bits == 0) { if (++w >= words) return limit; bits = o[w]; }
  return min(w * 32 + ctz(bits), limit);
}
/* One thread per (band, register). The count pass sizes the spans and map;
 * the write pass records span boundaries and initializes first_seen. */
/* Accel: BODY-COMPOSE metal */
kernel void body_spans(device const uint *occupancy [[buffer(0)]],
                       device uint *counts [[buffer(1)]],
                       device uint *widths [[buffer(2)]],
                       device const uint *node_first [[buffer(3)]],
                       device const uint *map_first [[buffer(4)]],
                       device uint4 *nodes [[buffer(5)]],
                       device uchar *map [[buffer(6)]],
                       device const uchar *indexes [[buffer(7)]],
                       device const uchar *mask [[buffer(8)]],
                       constant body_params &p [[buffer(9)]],
                       constant uint &write_pass [[buffer(10)]],
                       device uint *first_seen [[buffer(11)]],
                       device uint *node_band [[buffer(12)]],
                       uint t [[thread_position_in_grid]]) {
  device const uint *o;
  uint band;
  uint c;
  uint limit;
  uint n;
  uint used;
  uint node_base;
  uint map_base;
  uint sx;
  uint mx;
  uint nx;

  if (t >= p.bands * p.ncolors) return;
  o = occupancy + t * p.words;
  band = t / p.ncolors;
  c = t % p.ncolors;
  limit = p.encoded_width;
  n = 0;
  used = 0;
  node_base = write_pass != 0 ? node_first[t] : 0;
  map_base = write_pass != 0 ? map_first[t] : 0;
  sx = body_next(o, p.words, limit, 0);
  while (sx < limit) {
    mx = sx + 1;
    nx = body_next(o, p.words, limit, mx);
    while (nx < limit && nx - mx <= 9) {
      mx = nx + 1;
      nx = body_next(o, p.words, limit, mx);
    }
    if (write_pass != 0) {
      nodes[node_base + n] = uint4(c, sx, mx, map_base + used);
      node_band[node_base + n] = band;
    }
    used += mx - sx;
    n++;
    sx = nx;
  }
  if (write_pass == 0) { counts[t] = n; widths[t] = used; }
  else first_seen[t] = 0xffffffffu;
}
/* A lane builds one span's six-row map. Splitting by span removes the
 * serial six-row work from the (band, color) span discovery lane. */
/* Accel: BODY-COMPOSE metal */
kernel void body_map(device const uint4 *nodes [[buffer(0)]],
                     device const uint *node_band [[buffer(1)]],
                     device uchar *map [[buffer(2)]],
                     device const uchar *indexes [[buffer(3)]],
                     device const uchar *mask [[buffer(4)]],
                     device atomic_uint *first_seen [[buffer(5)]],
                     constant body_params &p [[buffer(6)]],
                     uint t [[thread_position_in_grid]]) {
  uint4 node;
  uint band;
  uint first;
  uint x;
  uint v;
  uint r;
  uint i;
  int sxp;
  int syp;

  if (t >= p.nodes) return;
  node = nodes[t];
  band = node_band[t];
  first = 0xffffffffu;
  for (x = node.y; x < node.z; x++) {
    v = 0;
    sxp = (int)x - (int)p.offset_left;
    if (sxp >= 0 && (uint)sxp < p.width) {
      for (r = 0; r < 6; r++) {
        syp = (int)(band * 6 + r) - (int)p.offset_top;
        if (syp < 0 || (uint)syp >= p.height) continue;
        i = (uint)syp * p.width + (uint)sxp;
        if (indexes[i] == node.x &&
            (p.has_mask == 0 || mask[i] == 0)) v |= 1u << r;
      }
    }
    if (v != 0) first = min(first, ctz(v) * p.encoded_width + x);
    map[node.w + x - node.y] = (uchar)v;
  }
  if (first != 0xffffffffu)
    atomic_fetch_min_explicit(&first_seen[band * p.ncolors + node.x],
                              first, memory_order_relaxed);
}
/* Emission. The spelling follows sixel_put_node: a run of at most three
 * cells repeats its symbol, a longer one is '!' count symbol, and the
 * limited form first peels '!255' symbol pieces from runs above 255. */
struct emit_params {
  uint encoded_width; uint words; uint ncolors; uint bands;
  uint selectors; uint limited; uint nodes; uint group_limit;
};
static uint emit_digits(uint v) {
  uint n = 1; while (v >= 10) { v /= 10; n++; } return n;
}
static uint emit_run_length(uint c, uint limited) {
  uint n = 0;
  if (limited != 0 && c > 255) { uint k = (c - 1) / 255; n = k * 5; c -= k * 255; }
  return n + (c <= 3 ? c : 2 + emit_digits(c));
}
static uint emit_number(device uchar *out, uint o, uint v) {
  uint n = emit_digits(v);
  for (uint i = n; i > 0; i--) { out[o + i - 1] = (uchar)('0' + v % 10); v /= 10; }
  return o + n;
}
static uint emit_run(device uchar *out, uint o, uchar s, uint c, uint limited) {
  if (limited != 0 && c > 255) {
    uint k = (c - 1) / 255; c -= k * 255;
    for (; k > 0; k--) { out[o] = '!'; out[o + 1] = '2'; out[o + 2] = '5'; out[o + 3] = '5'; out[o + 4] = s; o += 5; }
  }
  if (c <= 3) { for (; c > 0; c--) out[o++] = s; return o; }
  out[o++] = '!'; o = emit_number(out, o, c); out[o++] = s;
  return o;
}
/* One thread per span: the spelled length of its column values. */
/* Accel: BODY-SPELL metal */
kernel void body_lengths(device const uint4 *nodes [[buffer(0)]],
                         device const uchar *map [[buffer(1)]],
                         device uint *lengths [[buffer(2)]],
                         constant emit_params &p [[buffer(3)]],
                         uint t [[thread_position_in_grid]]) {
  if (t >= p.nodes) return;
  uint4 n = nodes[t];
  device const uchar *row = map + n.w;
  uint w = n.z - n.y; uint total = 0;
  for (uint i = 0; i < w;) {
    uchar v = row[i]; uint e = i + 1;
    while (e < w && row[e] == v) e++;
    total += emit_run_length(e - i, p.limited); i = e;
  }
  lengths[t] = total;
}
/* One thread per band reproduces sixel_band_emit. Each column holds its
 * spans by farther end, then later first-seen register; a sweep takes the
 * first remaining start at or after the cursor, and a new sweep begins
 * with '$' at the first remaining start. Each emitted span records its
 * node, its byte offset in the band, the cursor before it, and flags. */
/* Accel: BODY-SPELL metal */
kernel void body_order(device const uint4 *nodes [[buffer(0)]],
                       device const uint *band_first [[buffer(1)]],
                       device const uint *first_seen [[buffer(2)]],
                       device const uint *lengths [[buffer(3)]],
                       device const uint *registers [[buffer(4)]],
                       device uint *columns [[buffer(5)]],
                       device uint *bits [[buffer(6)]],
                       device uint *sorted [[buffer(7)]],
                       device uint4 *entries [[buffer(8)]],
                       device uint *band_size [[buffer(9)]],
                       constant emit_params &p [[buffer(10)]],
                       uint b [[thread_position_in_grid]]) {
  if (b >= p.bands) return;
  uint first = band_first[b]; uint end = band_first[b + 1];
  if (p.group_limit != 0 && end - first <= p.group_limit) return;
  uint width = p.encoded_width;
  device uint *head = columns + (ulong)b * (2 * width + 1);
  device uint *stop = head + width + 1;
  device uint *o = bits + (ulong)b * p.words;
  device const uint *rank = first_seen + (ulong)b * p.ncolors;
  for (uint x = 0; x <= width; x++) head[x] = 0;
  for (uint w = 0; w < p.words; w++) o[w] = 0;
  for (uint i = first; i < end; i++) head[nodes[i].y + 1]++;
  for (uint x = 0; x < width; x++) head[x + 1] += head[x];
  for (uint x = 0; x < width; x++) stop[x] = head[x];
  for (uint i = first; i < end; i++) {
    uint4 n = nodes[i]; uint k = first + stop[n.y]++;
    uint j = k;
    while (j > first + head[n.y]) {
      uint4 q = nodes[sorted[j - 1]];
      if (q.z > n.z || (q.z == n.z && rank[q.x] > rank[n.x])) break;
      sorted[j] = sorted[j - 1]; j--;
    }
    sorted[j] = i;
    o[n.y / 32] |= 1u << (n.y % 32);
  }
  uint pos = b > 0 ? 1 : 0; uint k = first; uint x = 0; uint active = 0xffffffffu;
  uint sx = body_next(o, p.words, width, 0);
  while (sx < width) {
    uint dollar = 0;
    if (x > sx) { dollar = 1; x = 0; }
    do {
      uint i = sorted[first + head[sx]];
      uint4 n = nodes[i];
      uint sel = p.selectors != 0 && active != n.x ? 1 : 0;
      entries[k++] = uint4(i, pos, x, dollar | sel << 1 | b << 2);
      pos += dollar + (sel != 0 ? 1 + emit_digits(registers[n.x]) : 0)
        + (x < sx ? emit_run_length(sx - x, p.limited) : 0) + lengths[i];
      if (sel != 0) active = n.x;
      dollar = 0; x = n.z;
      if (++head[sx] == stop[sx]) o[sx / 32] &= ~(1u << (sx % 32));
      sx = body_next(o, p.words, width, x);
    } while (sx < width);
    sx = body_next(o, p.words, width, 0);
  }
  band_size[b] = pos;
}
/* Bands of at most BODY_GROUP_NODES spans take one threadgroup each. The
 * threads sort the spans by start, farther end, then later first-seen
 * register, which is the column order of body_order; one thread then takes
 * the sweeps from threadgroup memory, and the threads sum the spelled
 * lengths into offsets. The first remaining span at or after the cursor is
 * the first unconsumed sorted span whose start reaches it, since each
 * column's spans are consumed from its head. */
#define BODY_GROUP_NODES 4096
#define BODY_GROUP_THREADS 256
#define BODY_GROUP_COLUMNS 2048
/* Accel: BODY-SPELL metal */
kernel void body_order_group(device const uint4 *nodes [[buffer(0)]],
                             device const uint *band_first [[buffer(1)]],
                             device const uint *first_seen [[buffer(2)]],
                             device const uint *lengths [[buffer(3)]],
                             device const uint *registers [[buffer(4)]],
                             device ulong *keys [[buffer(5)]],
                             device ushort *slots [[buffer(6)]],
                             device uint4 *entries [[buffer(7)]],
                             device uint *band_size [[buffer(8)]],
                             constant emit_params &p [[buffer(9)]],
                             uint b [[threadgroup_position_in_grid]],
                             uint t [[thread_position_in_threadgroup]]) {
  threadgroup uint spans[BODY_GROUP_NODES];
  threadgroup uchar colors[BODY_GROUP_NODES];
  threadgroup uint consumed[BODY_GROUP_NODES / 32];
  threadgroup uint partial[BODY_GROUP_THREADS];
  threadgroup ushort column[BODY_GROUP_COLUMNS + 1];
  uint first = band_first[b]; uint n = band_first[b + 1] - first;
  if (n > p.group_limit) return;
  uint size = 1; while (size < n) size <<= 1;
  device ulong *k = keys + (ulong)b * BODY_GROUP_NODES;
  device ushort *s = slots + (ulong)b * BODY_GROUP_NODES;
  device const uint *rank = first_seen + (ulong)b * p.ncolors;
  for (uint i = t; i < size; i += BODY_GROUP_THREADS) {
    if (i < n) {
      uint4 q = nodes[first + i];
      k[i] = (ulong)q.y << 48 | (ulong)(0xffffu - q.z) << 32 | (ulong)(0xffffffffu - rank[q.x]);
    } else k[i] = ~0ul;
    s[i] = (ushort)i;
  }
  for (uint i = t; i < BODY_GROUP_NODES / 32; i += BODY_GROUP_THREADS) consumed[i] = 0;
  threadgroup_barrier(mem_flags::mem_device);
  for (uint w = 2; w <= size; w <<= 1) {
    for (uint j = w >> 1; j > 0; j >>= 1) {
      for (uint i = t; i < size; i += BODY_GROUP_THREADS) {
        uint l = i ^ j;
        if (l > i) {
          ulong a = k[i]; ulong c = k[l];
          bool up = (i & w) == 0;
          if (up ? a > c : a < c) {
            k[i] = c; k[l] = a; ushort v = s[i]; s[i] = s[l]; s[l] = v;
          }
        }
      }
      threadgroup_barrier(mem_flags::mem_device);
    }
  }
  for (uint i = t; i < n; i += BODY_GROUP_THREADS) {
    uint4 q = nodes[first + s[i]];
    spans[i] = q.y << 16 | q.z; colors[i] = (uchar)q.x;
  }
  threadgroup_barrier(mem_flags::mem_threadgroup);
/* With the frame no wider than the table, column[x] is the first sorted
 * position whose start reaches x; wider frames search for it. */
  bool table = p.encoded_width <= BODY_GROUP_COLUMNS;
  for (uint i = t; table && i <= n; i += BODY_GROUP_THREADS) {
    uint lo = i > 0 ? (spans[i - 1] >> 16) + 1 : 0;
    uint hi = i < n ? spans[i] >> 16 : p.encoded_width;
    for (uint x = lo; x <= hi; x++) column[x] = (ushort)i;
  }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if (t == 0) {
    uint x = 0; uint e = 0; uint low = 0; uint active = 0xffffffffu;
    while (true) {
      while (low < n && (consumed[low / 32] >> (low % 32) & 1) != 0) low++;
      if (low >= n) break;
      uint at = low; uint dollar = 0;
      if (x > (spans[at] >> 16)) { dollar = 1; x = 0; }
      while (true) {
        uint sx = spans[at] >> 16; uint mx = spans[at] & 0xffffu;
        uint c = colors[at];
        uint sel = p.selectors != 0 && active != c ? 1 : 0;
        entries[first + e++] = uint4(first + s[at], 0, x, dollar | sel << 1 | b << 2);
        if (sel != 0) active = c;
        consumed[at / 32] |= 1u << (at % 32);
        dollar = 0; x = mx;
        uint lo = at + 1;
        if (table) lo = column[x];
        else { uint hi = n; while (lo < hi) { uint m = (lo + hi) / 2; if ((spans[m] >> 16) < x) lo = m + 1; else hi = m; } }
        uint w = lo / 32; uint bits = lo < n ? ~consumed[w] & (0xffffffffu << (lo % 32)) : 0;
        while (bits == 0 && ++w * 32 < n) bits = ~consumed[w];
        if (bits == 0) break;
        at = w * 32 + ctz(bits);
        if (at >= n) break;
      }
    }
  }
  threadgroup_barrier(mem_flags::mem_device);
  uint per = (n + BODY_GROUP_THREADS - 1) / BODY_GROUP_THREADS;
  uint lo = t * per; uint hi = min(n, lo + per); uint sum = 0;
  for (uint i = lo; i < hi; i++) {
    uint4 e = entries[first + i]; uint4 q = nodes[e.x];
    uint len = (e.w & 1) + ((e.w & 2) != 0 ? 1 + emit_digits(registers[q.x]) : 0)
      + (e.z < q.y ? emit_run_length(q.y - e.z, p.limited) : 0) + lengths[e.x];
    spans[i] = len; sum += len;
  }
  partial[t] = sum;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if (t == 0) {
    uint run = b > 0 ? 1 : 0;
    for (uint i = 0; i < BODY_GROUP_THREADS; i++) { uint v = partial[i]; partial[i] = run; run += v; }
    band_size[b] = run;
  }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  uint pos = partial[t];
  for (uint i = lo; i < hi; i++) { entries[first + i].y = pos; pos += spans[i]; }
}
/* Band offsets and the '-' that opens every band after the first. */
/* Accel: BODY-SPELL metal */
kernel void body_bases(device const uint *band_size [[buffer(0)]],
                       device uint *band_bytes [[buffer(1)]],
                       device uchar *out [[buffer(2)]],
                       constant emit_params &p [[buffer(3)]],
                       uint t [[thread_position_in_grid]]) {
  if (t != 0) return;
  uint total = 0;
  for (uint b = 0; b < p.bands; b++) {
    band_bytes[b] = total;
    if (b > 0) out[total] = '-';
    total += band_size[b];
  }
  band_bytes[p.bands] = total;
}
/* Accel: BODY-SPELL metal */
kernel void body_write(device const uint4 *nodes [[buffer(0)]],
                       device const uchar *map [[buffer(1)]],
                       device const uint4 *entries [[buffer(2)]],
                       device const uint *band_bytes [[buffer(3)]],
                       device const uint *registers [[buffer(4)]],
                       device uchar *out [[buffer(5)]],
                       constant emit_params &p [[buffer(6)]],
                       uint t [[thread_position_in_grid]]) {
  if (t >= p.nodes) return;
  uint4 e = entries[t];
  uint4 n = nodes[e.x];
  uint o = band_bytes[e.w >> 2] + e.y;
  if ((e.w & 1) != 0) out[o++] = '$';
  if ((e.w & 2) != 0) { out[o++] = '#'; o = emit_number(out, o, registers[n.x]); }
  if (e.z < n.y) o = emit_run(out, o, '?', n.y - e.z, p.limited);
  device const uchar *row = map + n.w;
  uint w = n.z - n.y;
  for (uint i = 0; i < w;) {
    uchar v = row[i]; uint f = i + 1;
    while (f < w && row[f] == v) f++;
    o = emit_run(out, o, (uchar)('?' + v), f - i, p.limited); i = f;
  }
}
