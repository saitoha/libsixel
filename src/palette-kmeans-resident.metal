/* SPDX-License-Identifier: MIT */
struct Geometry {
    uint width, height, stride, colors, min_iterations;
    float threshold;
};
/* These are the byte-cover contract from palette-common-cover.h. */
#define COVER_SHARE 1024u
#define COVER_MIN 2u
#define COVER_NEAR 1728u
#define COVER_MARGIN 2u
#define COVER_ROUNDS 4u
// The host binds persistent resources and clears the histogram and state.
struct Cell {
    atomic_uint v[4];
};
#define ARG(i) [[buffer(i)]]
#define GID uint i [[thread_position_in_grid]]
static inline uint2 grid_position(uint i, constant Geometry &geometry)
{
    uint stride = geometry.stride;
    uint width = (geometry.width + stride - 1) / stride;
    return uint2(i % width, i / width) * stride;
}

/* Accel: KMEANS-RESIDENT-HISTOGRAM metal */
kernel void histogram(device const packed_uchar3 *rgb ARG(10),
                      device Cell *bins ARG(11),
                      constant Geometry &geometry ARG(12), GID)
{
    uint2 xy = grid_position(i, geometry);
    if (xy.y >= geometry.height)
        return;
    uint3 c = uint3(uchar3(rgb[xy.y * geometry.width + xy.x]));
    uint3 b = min(c * 32 / 255, uint3(31));
    uint key = (b.x << 10) + (b.y << 5) + b.z;
    for (uint j = 0; j < 3; ++j)
        atomic_fetch_add_explicit(&bins[key].v[j], c[j], memory_order_relaxed);
    atomic_fetch_add_explicit(&bins[key].v[3], 1u, memory_order_relaxed);
}
// Each lane owns a disjoint key range. Stable key order makes repeats exact.
/* Accel: KMEANS-RESIDENT-HISTOGRAM metal */
kernel void count_bins(device const uint4 *bins ARG(11),
                       device uint *offset ARG(13), GID)
{
    uint count = 0;
    for (uint j = 0; j < 128; ++j)
        count += bins[i * 128 + j].w != 0;
    offset[i] = count;
}
/* Accel: KMEANS-RESIDENT-HISTOGRAM metal */
kernel void prefix(device uint *offset ARG(13), device Params &p ARG(9),
                   device uint *dispatch ARG(14),
                   constant Geometry &geometry ARG(12), GID)
{
    uint n = 0;
    for (uint j = 0; j < 256; ++j) {
        uint count = offset[j];
        offset[j] = n;
        n += count;
    }
    p.points = n;
    p.centers = min(n, geometry.colors);
    p.groups = (n + 511) / 512;
    p.min_iterations = geometry.min_iterations;
    p.round_centers = 1;
    p.threshold = geometry.threshold;
    dispatch[0] = (n + 127) / 128;
    dispatch[3] = p.groups;
    dispatch[1] = dispatch[2] = dispatch[4] = dispatch[5] = 1;
}
/* Accel: KMEANS-RESIDENT-HISTOGRAM metal */
kernel void compact(device const uint4 *bins ARG(11),
                    device const uint *offset ARG(13),
                    device packed_float3 *pts ARG(0), device float *w ARG(1),
                    GID)
{
    uint out = offset[i];
    for (uint j = 0; j < 128; ++j) {
        uint4 cell = bins[i * 128 + j];
        if (!cell.w)
            continue;
        w[out] = cell.w;
        pts[out++] = float3(cell.xyz) / float(cell.w);
    }
}
struct Moment {
    float4 s, diag, cross;
};
/* Accel: KMEANS-RESIDENT-PCA metal */
kernel void moments(device const packed_float3 *pts ARG(0),
                    device const float *w ARG(1), device Moment *part ARG(15),
                    constant Params &p ARG(9), GID)
{
    Moment m = {};
    for (uint j = i * 64; j < min(i * 64 + 64, p.points); ++j) {
        float3 a = float3(pts[j]) - 128.0f;
        m.s += float4(a, 1) * w[j];
        m.diag.xyz += a * a * w[j];
        m.cross.xyz += a * a.yzx * w[j];
    }
    part[i] = m;
}
/* Accel: KMEANS-RESIDENT-PCA metal */
kernel void axis(device const Moment *part ARG(15), device float4 *v ARG(16),
                 constant Params &p ARG(9), GID)
{
    Moment m = {};
    for (uint j = 0; j < (p.points + 63) / 64; ++j) {
        m.s += part[j].s;
        m.diag += part[j].diag;
        m.cross += part[j].cross;
    }
    float3 d = m.diag.xyz - m.s.xyz * m.s.xyz / m.s.w;
    float3 c = m.cross.xyz - m.s.xyz * m.s.yzx / m.s.w;
    float3 a = 1;
    for (uint j = 0; j < 16; ++j) {
        float3 q = d * a + c * a.yzx + c.zxy * a.zxy;
        if (dot(q, q) == 0)
            break;
        a = normalize(q);
    }
    v[0] = float4(a, m.s.w);
}
struct Key {
    float proj;
    uint idx;
};
bool less_key(Key a, Key b)
{
    return a.proj < b.proj || (a.proj == b.proj && a.idx < b.idx);
}
/* Accel: KMEANS-RESIDENT-PCA metal */
kernel void tiles(device const packed_float3 *pts ARG(0),
                  device const float4 *v ARG(16), device Key *out ARG(17),
                  constant Params &p ARG(9), GID,
                  uint t [[thread_index_in_threadgroup]])
{
    threadgroup Key a[128];
    Key k = {INFINITY, i};
    if (i < p.points)
        k.proj = dot(float3(pts[i]), v[0].xyz);
    a[t] = k;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint n = 2; n <= 128; n *= 2)
        for (uint h = n / 2; h; h /= 2) {
            uint u = t ^ h;
            if (t < u && less_key(a[u], a[t]) != bool(t & n)) {
                Key tmp = a[t];
                a[t] = a[u];
                a[u] = tmp;
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
    out[i] = a[t];
}
// Merge adjacent sorted runs by binary-searching each key's other run.
/* Accel: KMEANS-RESIDENT-PCA metal */
kernel void merge_keys(device Key *out ARG(18), device const Key *in ARG(17),
                       constant Params &p ARG(9), constant uint &n ARG(19), GID)
{
    if (i >= p.points)
        return;
    uint base = (i / n) * n, other = base ^ n, lo = other,
         hi = min(other + n, p.points);
    Key key = in[i];
    while (lo < hi) {
        uint mid = (lo + hi) / 2;
        if (less_key(in[mid], key))
            lo = mid + 1;
        else
            hi = mid;
    }
    out[min(base, other) + (i - base) + (lo - other)] = key;
}
/* Accel: KMEANS-RESIDENT-PCA metal */
kernel void seed(device const packed_float3 *pts ARG(0),
                 device const float *w ARG(1),
                 device packed_float3 *centers ARG(2),
                 device const Key *keys ARG(17), device const float4 *v ARG(16),
                 constant Params &p ARG(9), GID)
{
    uint cursor = 0;
    float cumulative = 0;
    for (uint c = 0; c < p.centers; ++c) {
        float4 sum = 0;
        float end = v[0].w * (c + 1) / p.centers;
        while (cursor < p.points && cumulative < end) {
            uint j = keys[cursor++].idx;
            sum += float4(float3(pts[j]), 1) * w[j];
            cumulative += w[j];
        }
        float3 mean =
            sum.w > 0 ? sum.xyz / sum.w : float3(pts[keys[cursor - 1].idx]);
        centers[c] = floor(mean + 0.5f);
    }
}
// A single restart with no merge has no consumer for an SSE ranking.
// Compute the final objective anyway so the measured path includes rescoring.
/* Accel: KMEANS-RESIDENT-OBJECTIVE metal */
kernel void score(device const packed_float3 *pts ARG(0),
                  device const packed_float3 *centers ARG(2),
                  device const float *w ARG(1), device float *dist ARG(4),
                  constant Params &p ARG(9), GID)
{
    if (i >= p.points)
        return;
    float best = INFINITY;
    for (uint c = 0; c < p.centers; ++c) {
        float3 delta = float3(pts[i]) - float3(centers[c]);
        best = min(best, dot(delta, delta));
    }
    dist[i] = best * w[i];
}
/* Accel: KMEANS-RESIDENT-OBJECTIVE metal */
kernel void objective(device const float *dist ARG(4),
                      device float *sse ARG(20), constant Params &p ARG(9), GID)
{
    float sum = 0;
    for (uint j = 0; j < p.points; ++j)
        sum += dist[j];
    *sse = sum;
}
/* Accel: KMEANS-RESIDENT-PACK metal */
kernel void pack(device const packed_float3 *centers ARG(2),
                 device packed_uchar3 *pal ARG(21), constant Params &p ARG(9),
                 GID)
{
    if (i < p.centers)
        pal[i] = uchar3(float3(centers[i]));
}
/* Accel: KMEANS-RESIDENT-LOOKUP metal */
kernel void apply_palette(device const packed_uchar3 *rgb ARG(10),
                          device const uchar *lut ARG(22),
                          device uchar *idx ARG(23),
                          constant Geometry &geometry ARG(12), GID)
{
    if (i >= geometry.width * geometry.height)
        return;
    idx[i] = lookup_6bit_palette_lookup(lut, uchar3(rgb[i]));
}

// Gamma RGB888 final snap. The immutable byte table uses the host's existing
// percentage lattice, approach rate and exact byte-rounding function.
/* Accel: KMEANS-RESIDENT-SNAP metal */
kernel void final_snap(device packed_uchar3 *pal ARG(21),
                       constant uchar *map ARG(25), constant Params &p ARG(9),
                       GID)
{
    if (i < p.centers) {
        uchar3 c = uchar3(pal[i]);
        pal[i] = uchar3(map[c.x], map[c.y], map[c.z]);
    }
}

uint sq_rgb(uint3 a, uint3 b)
{
    int3 d = int3(a) - int3(b);
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}

uint3 cover_color(uint4 cell) { return cell.xyz / cell.w; }

// The 4-bit source histogram is distinct from the Lloyd histogram. It
// consumes the same sampled RGB pixels as the byte cover policy.
/* Accel: KMEANS-RESIDENT-COVER metal */
kernel void cover_histogram(device const packed_uchar3 *rgb ARG(10),
                            device Cell *bins ARG(26),
                            constant Geometry &geometry ARG(12), GID)
{
    uint2 xy = grid_position(i, geometry);
    if (xy.y >= geometry.height)
        return;
    uint3 c = uint3(uchar3(rgb[xy.y * geometry.width + xy.x])), b = c >> 4;
    uint key = (b.x << 8) | (b.y << 4) | b.z;
    for (uint j = 0; j < 3; ++j)
        atomic_fetch_add_explicit(&bins[key].v[j], c[j], memory_order_relaxed);
    atomic_fetch_add_explicit(&bins[key].v[3], 1u, memory_order_relaxed);
}

/* Accel: KMEANS-RESIDENT-COVER metal */
kernel void cover_gaps(device const uint4 *bins ARG(26),
                       device uint *gaps ARG(27),
                       device const packed_uchar3 *pal ARG(21),
                       constant Params &p ARG(9),
                       constant uint &samples ARG(19), GID)
{
    uint4 cell = bins[i];
    uint gap = 0;
    if (cell.w >= max(samples / COVER_SHARE, COVER_MIN)) {
        uint3 c = cover_color(cell);
        gap = UINT_MAX;
        for (uint k = 0; k < p.centers; ++k)
            gap = min(gap, sq_rgb(c, uint3(uchar3(pal[k]))));
    }
    gaps[i] = gap;
}

uint cover_budget(uint policy, uint count)
{
    if (policy == 4u)
        return count < 32 ? 0u : count < 64 ? 8u : count < 256 ? 14u : 26u;
    return policy == 1 ? 8u : policy == 2 ? 14u : policy == 3 ? 26u : 0u;
}

uint anchor_gap(uint3 c, threadgroup uint3 *pal, uint count)
{
    uint gap = UINT_MAX;
    for (uint j = 0; j < count; ++j)
        gap = min(gap, sq_rgb(c, pal[j]));
    return gap;
}

// One workgroup owns the palette, applying the same sequential changes
// as CPU cover. Rank candidates and pairs in parallel with CPU tie order.
/* Accel: KMEANS-RESIDENT-COVER metal */
kernel void cover_repair(device packed_uchar3 *pal ARG(21),
                         device Params &p ARG(9),
                         constant uint4 &policy ARG(24),
                         constant packed_uchar3 *fixed ARG(28),
                         device const uint4 *bins ARG(26),
                         device uint *gaps ARG(27),
                         uint lane [[thread_index_in_threadgroup]])
{
    threadgroup uint3 colors[256], anchors[26];
    threadgroup ulong best[256];
    threadgroup uint count, found, placed;
    uint limit = cover_budget(policy.y, p.centers);
    if (lane < p.centers)
        colors[lane] = uint3(uchar3(pal[lane]));
    if (lane == 0) {
        count = p.centers;
        found = 0;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (policy.z) {
        for (uint a = 0; a < limit; ++a) {
            ulong rank = 0;
            for (uint j = lane; j < 4096; j += 256)
                rank = max(rank, ulong((gaps[j] << 12) | (4095 - j)));
            best[lane] = rank;
            threadgroup_barrier(mem_flags::mem_threadgroup);
            for (uint n = 128; n; n /= 2) {
                if (lane < n)
                    best[lane] = max(best[lane], best[lane + n]);
                threadgroup_barrier(mem_flags::mem_threadgroup);
            }
            if ((best[0] >> 12) <= COVER_NEAR)
                break;
            uint key = 4095 - (best[0] & 4095);
            uint3 c = cover_color(bins[key]);
            if (lane == 0) {
                anchors[a] = c;
                ++found;
            }
            for (uint j = lane; j < 4096; j += 256) {
                uint gap = gaps[j];
                if (gap > COVER_NEAR)
                    gaps[j] = min(gap, sq_rgb(c, cover_color(bins[j])));
            }
            threadgroup_barrier(mem_flags::mem_device_and_threadgroup);
        }
        if (lane == 0 && policy.w) {
            uint room = min(found, 256 - count);
            for (uint j = 0; j < room; ++j)
                colors[count++] = anchors[j];
        }
    } else if (lane == 0) {
        if (policy.w) {
            // Growth tests every anchor against the original prefix only.
            uint original = count;
            for (uint a = 0; a < limit && count < 256; ++a) {
                uint3 c = uint3(uchar3(fixed[a]));
                if (anchor_gap(c, colors, original) > COVER_NEAR)
                    colors[count++] = c;
            }
            limit = cover_budget(policy.y, count);
        }
        for (uint a = 0; a < limit; ++a) {
            uint3 c = uint3(uchar3(fixed[a]));
            if (anchor_gap(c, colors, count) > COVER_NEAR)
                anchors[found++] = c;
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint r = 0; r < COVER_ROUNDS; ++r) {
        if (lane == 0)
            placed = 0;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        for (uint a = 0; a < found; ++a) {
            best[lane] =
                lane < count ? sq_rgb(anchors[a], colors[lane]) : UINT_MAX;
            threadgroup_barrier(mem_flags::mem_threadgroup);
            for (uint n = 128; n; n /= 2) {
                if (lane < n)
                    best[lane] = min(best[lane], best[lane + n]);
                threadgroup_barrier(mem_flags::mem_threadgroup);
            }
            uint gap = best[0];
            // All lanes must read the gap before lane zero reuses best[0].
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (gap <= COVER_NEAR)
                continue;
            ulong rank = ULONG_MAX;
            if (lane < count) {
                for (uint j = lane + 1; j < count; ++j) {
                    ulong distance = sq_rgb(colors[lane], colors[j]);
                    rank = min(rank, (distance << 16) | (lane << 8) | j);
                }
            }
            best[lane] = rank;
            threadgroup_barrier(mem_flags::mem_threadgroup);
            for (uint n = 128; n; n /= 2) {
                if (lane < n)
                    best[lane] = min(best[lane], best[lane + n]);
                threadgroup_barrier(mem_flags::mem_threadgroup);
            }
            if (lane == 0 && best[0] != ULONG_MAX &&
                (best[0] >> 16) <= gap / COVER_MARGIN) {
                uint left = uint((best[0] >> 8) & 255);
                uint right = uint(best[0] & 255);
                colors[left] = (colors[left] + colors[right]) / 2;
                colors[right] = anchors[a];
                ++placed;
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
        if (!placed)
            break;
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    if (lane < count)
        pal[lane] = uchar3(colors[lane]);
    if (lane == 0)
        p.centers = count;
}

#if defined(SIXEL_HECKBERT_RESIDENT)
#define HB_TILE 128u

static inline uint hb_resident_key(uint3 snapped)
{
    uint3 bin = min((snapped + 4u) >> 2u, uint3(63u));
    return (bin.x << 12u) | (bin.y << 6u) | bin.z;
}

/* Map the GPU grid directly onto the input RGB buffer. Record the first grid
 * index in each reversible 6bit cell so parallel insertion cannot reorder
 * equal-priority boxes or the direct palette for a low-color image. */
/* Accel: HECKBERT-RESIDENT-REP metal */
kernel void heckbert_histogram_first(device atomic_uint *first ARG(6),
    device uint *samples ARG(8), device const packed_uchar3 *rgb ARG(10),
    constant Geometry &geometry ARG(12), constant uchar *tones ARG(20), GID)
{
    uint2 xy = grid_position(i, geometry);
    if (xy.y >= geometry.height)
        return;
    uchar3 c = uchar3(rgb[xy.y * geometry.width + xy.x]);
    uint3 snapped = uint3(tones[c.x], tones[c.y], tones[c.z]);
    samples[i] = (snapped.x << 16u) | (snapped.y << 8u) | snapped.z;
    atomic_fetch_min_explicit(&first[hb_resident_key(snapped)], i,
                              memory_order_relaxed);
}

/* Use the first occurrence as a dense index into sample-sized moment
 * storage. The largest possible channel sum is 32768 * 255. */
/* Accel: HECKBERT-RESIDENT-REP metal */
kernel void heckbert_histogram_accumulate(device const uint *first ARG(6),
    device Cell *moments ARG(7), device const uint *samples ARG(8),
    device atomic_uint *tiles ARG(9), constant uint &total ARG(19), GID)
{
    if (i >= total)
        return;
    uint packed = samples[i];
    uint3 snapped = uint3(packed >> 16u, (packed >> 8u) & 255u, packed & 255u);
    uint index = first[hb_resident_key(snapped)];
    for (uint axis = 0u; axis < 3u; ++axis)
        atomic_fetch_add_explicit(&moments[index].v[axis], snapped[axis],
                                  memory_order_relaxed);
    atomic_fetch_add_explicit(&moments[index].v[3], 1u, memory_order_relaxed);
    if (i == index)
        atomic_fetch_add_explicit(&tiles[i / HB_TILE], 1u,
                                  memory_order_relaxed);
}

/* Accel: HECKBERT-RESIDENT-REP metal */
kernel void heckbert_histogram_prefix(device HeckbertParams &p ARG(4),
    device uint *tiles ARG(9), constant Geometry &geometry ARG(12),
    constant uint &total ARG(19))
{
    uint count = 0u;
    for (uint tile = 0u; tile < (total + HB_TILE - 1u) / HB_TILE; ++tile) {
        uint occupied = tiles[tile];
        tiles[tile] = count;
        count += occupied;
    }
    p = {count, geometry.colors, total, 1.0f, 1.0f, 1.0f};
}

/* One SIMD group per tile compacts the weighted points in grid order. As
 * the pre-snap input sums are integral, half-up rounding is exact. */
/* Accel: HECKBERT-RESIDENT-REP metal */
kernel void heckbert_histogram_compact(device float4 *points ARG(0),
    device const uint4 *moments ARG(7), device const uint *tiles ARG(9),
    constant uint &total ARG(19), constant uchar *tones ARG(20),
    uint tile [[threadgroup_position_in_grid]],
    uint lane [[thread_index_in_simdgroup]],
    uint width [[threads_per_simdgroup]])
{
    uint out = tiles[tile];
    uint end = min(total, (tile + 1u) * HB_TILE);
    for (uint base = tile * HB_TILE; base < end; base += width) {
        uint i = base + lane;
        uint4 cell = i < end ? moments[i] : uint4(0u);
        uint occupied = cell.w != 0u;
        uint position = out + simd_prefix_exclusive_sum(occupied);
        if (occupied) {
            uint3 c = (2u * cell.xyz + cell.w) / (2u * cell.w);
            points[position] = float4(tones[c.x], tones[c.y], tones[c.z],
                                      cell.w);
        }
        out += simd_sum(occupied);
    }
}
#undef HB_TILE

/* Match sumcompare and averagePixels on the reversible byte histogram.
 * The box priorities and membership come from the original Heckbert
 * solver. With at most 32768 samples, doubled RGB sums fit in uint. */
/* Accel: HECKBERT-RESIDENT-REP metal */
kernel void heckbert_representatives(
    device const float4 *points [[buffer(0)]],
    device const uint *order [[buffer(1)]],
    device const HeckbertBox *boxes [[buffer(3)]],
    device const HeckbertParams &input [[buffer(4)]],
    device const uint *solved [[buffer(5)]],
    device State &state [[buffer(7)]],
    device Params &p [[buffer(9)]],
    device packed_uchar3 *palette [[buffer(21)]], GID)
{
    /* With few enough buckets the byte CPU builder preserves histogram order
     * and uses each bucket directly. It never runs the split kernel. */
    uint count = min(input.count, input.colors);
    if (i == 0)
        p.centers = count;
    if (i >= count)
        return;
    if (input.count <= input.colors) {
        palette[i] = uchar3(points[i].xyz);
        return;
    }
    if (solved[0] != count) {
        atomic_store_explicit(&state.failed, 1u, memory_order_relaxed);
        return;
    }
    HeckbertBox box = boxes[i];
    uint rank = 0;
    uint population = 0;
    uint3 sums = 0;
    if (!box.size || box.start >= input.count || !isfinite(box.priority) ||
            box.size > input.count - box.start) {
        atomic_store_explicit(&state.failed, 1u, memory_order_relaxed);
        return;
    }
    for (uint j = 0; j < count; ++j) {
        HeckbertBox other = boxes[j];
        rank += other.priority > box.priority ||
                (other.priority == box.priority && other.start < box.start);
    }
    for (uint j = 0; j < box.size; ++j) {
        uint index = order[box.start + j];
        if (index >= input.count) {
            atomic_store_explicit(&state.failed, 1u, memory_order_relaxed);
            return;
        }
        float4 point = points[index];
        uint mass = uint(point.w);
        sums += uint3(point.xyz) * mass;
        population += mass;
    }
    if (!population || population != box.sum) {
        atomic_store_explicit(&state.failed, 1u, memory_order_relaxed);
        return;
    }
    palette[rank] = uchar3((2 * sums + population) / (2 * population));
}
#endif
