/* SPDX-License-Identifier: MIT */
/* Byte histogram moments fit exactly in 32 bits at the resident sample
 * limit. Keep centroids as (RGB sum, mass) through replay and Lloyd. */
#define WARD_POINTS 32768u
#define WARD_SLOTS 768u
struct WardStats {
    uint4 weighted, distinct, low, high;
};
struct WardAccum {
    atomic_uint weight[4], distinct[4], low[4], high[4];
};
struct WardWork {
    uint4 points[WARD_POINTS];
    uint owner[WARD_POINTS];
    float residual[WARD_POINTS];
    WardStats clusters[WARD_SLOTS];
    WardAccum accum[WARD_SLOTS];
    uint points_count, initial, active, pass, stopped, merges, use_stats;
};
#define WARG(n) [[buffer(n)]]
#define WID uint i [[thread_position_in_grid]]
static WardStats ward_empty()
{
    return {uint4(0u), uint4(0u), uint4(255u), uint4(0u)};
}
static WardStats ward_add(WardStats a, uint4 b)
{
    uint3 color = b.xyz / b.w;
    a.weighted += b;
    a.distinct += uint4(color, 1u);
    a.low.xyz = min(a.low.xyz, color);
    a.high.xyz = max(a.high.xyz, color);
    return a;
}
static float3 ward_mean(uint4 x)
{
    return float3(x.xyz) / float(x.w);
}
static void ward_accumulate(device WardAccum &a, uint4 b, bool stats)
{
    uint3 color = b.xyz / b.w;
    for (uint c = 0; c < 4; ++c)
        atomic_fetch_add_explicit(&a.weight[c], b[c], memory_order_relaxed);
    if (stats) {
        for (uint c = 0; c < 3; ++c) {
            atomic_fetch_add_explicit(&a.distinct[c], color[c],
                                     memory_order_relaxed);
            atomic_fetch_min_explicit(&a.low[c], color[c],
                                     memory_order_relaxed);
            atomic_fetch_max_explicit(&a.high[c], color[c],
                                     memory_order_relaxed);
        }
        atomic_fetch_add_explicit(&a.distinct[3], 1u, memory_order_relaxed);
    }
}
static WardStats ward_load(device WardAccum &a, bool stats)
{
    WardStats out = ward_empty();
    for (uint c = 0; c < 4; ++c)
        out.weighted[c] = atomic_load_explicit(&a.weight[c],
                                             memory_order_relaxed);
    if (stats) {
        for (uint c = 0; c < 4; ++c) {
            out.distinct[c] = atomic_load_explicit(&a.distinct[c],
                                                 memory_order_relaxed);
            out.low[c] = atomic_load_explicit(&a.low[c], memory_order_relaxed);
            out.high[c] = atomic_load_explicit(&a.high[c],
                                               memory_order_relaxed);
        }
    }
    return out;
}
static void ward_clear(device WardAccum &a, bool stats)
{
    for (uint c = 0; c < 4; ++c) {
        atomic_store_explicit(&a.weight[c], 0u, memory_order_relaxed);
        if (stats) {
            atomic_store_explicit(&a.distinct[c], 0u, memory_order_relaxed);
            atomic_store_explicit(&a.low[c], 255u, memory_order_relaxed);
            atomic_store_explicit(&a.high[c], 0u, memory_order_relaxed);
        }
    }
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_km_points(device const uint4 *bins WARG(11),
    device const uint *offsets WARG(13), device WardWork &w WARG(30), WID)
{
    uint out = offsets[i];
    for (uint j = 0; j < 128; ++j) {
        const uint4 cell = bins[i * 128 + j];
        if (cell.w != 0u)
            w.points[out++] = cell;
    }
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_km_init(device const Params &p WARG(9),
    device State &s WARG(7), device WardWork &w WARG(30),
    constant Geometry &geometry WARG(12), WID)
{
    if (i < geometry.colors)
        ward_clear(w.accum[i], false);
    if (i == 0) {
        w.points_count = p.points;
        w.initial = p.centers;
        w.use_stats = 0;
        w.stopped = 0;
        w.pass = 0;
        // Refresh ownership against the final oversplit centers.
        s.done = 0;
    }
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_km_collect(device const uint *owner WARG(3),
    device WardWork &w WARG(30), WID)
{
    if (i < w.points_count)
        ward_accumulate(w.accum[owner[i]], w.points[i], false);
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_km_clusters(
    device const packed_float3 *centers WARG(2),
    device WardWork &w WARG(30), constant Geometry &geometry WARG(12), WID)
{
    WardStats c = ward_empty();
    if (i < w.initial) {
        if (w.initial <= geometry.target)
            c.weighted = uint4(uint3(float3(centers[i])), 1u);
        else {
            c = ward_load(w.accum[i], false);
            if (!c.weighted.w)
                c.weighted.w = 1u;
        }
    }
    w.clusters[i] = c;
}

#if defined(SIXEL_HECKBERT_RESIDENT)
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_hb_points(device const float4 *points WARG(0),
    device const HeckbertParams &p WARG(4), device WardWork &w WARG(30), WID)
{
    if (i < p.count) {
        uint4 entry = uint4(points[i]);
        w.points[i] = uint4(entry.xyz * entry.w, entry.w);
    }
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_hb_clusters(device const uint *order WARG(1),
    device const HeckbertBox *boxes WARG(3),
    device const HeckbertParams &p WARG(4), device const uint *solved WARG(5),
    device State &s WARG(7), constant Geometry &geometry WARG(12),
    device WardWork &w WARG(30), WID)
{
    uint count = min(p.count, p.colors);
    if (i == 0) {
        w.points_count = p.count;
        w.initial = count;
        w.use_stats = geometry.representative != 0u;
        w.stopped = 0;
        w.pass = 0;
    }
    if (i >= count) {
        w.clusters[i] = ward_empty();
        return;
    }
    if (p.count <= p.colors) {
        w.clusters[i] = ward_add(ward_empty(), w.points[i]);
        return;
    }
    HeckbertBox box = boxes[i];
    if (solved[0] != count || !box.size || box.start >= p.count ||
            box.size > p.count - box.start || !isfinite(box.priority)) {
        atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);
        return;
    }
    WardStats out = ward_empty();
    uint rank = 0;
    for (uint k = 0; k < count; ++k)
        rank += boxes[k].priority > box.priority ||
            (boxes[k].priority == box.priority && boxes[k].start < box.start);
    for (uint j = box.start; j < box.start + box.size; ++j) {
        if (order[j] >= p.count) {
            atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);
            return;
        }
        out = ward_add(out, w.points[order[j]]);
    }
    if (out.weighted.w != box.sum) {
        atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);
        return;
    }
    w.clusters[rank] = out;
}
#endif

/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_seed(device float4 *means WARG(29),
    device WardParameters &p WARG(28), device WardWork &w WARG(30),
    constant Geometry &geometry WARG(12), device const State &s WARG(7), WID)
{
    bool failed = atomic_load_explicit(&s.failed, memory_order_relaxed) != 0;
    uint4 c = !failed && i < w.initial ? w.clusters[i].weighted : uint4(0);
    means[i] = c.w ? float4(ward_mean(c), c.w) : float4(0);
    if (i == 0) {
        w.active = failed ? 0u : min(w.initial, geometry.target);
        w.merges = failed ? 0u : w.initial - w.active;
        p = {geometry.colors, w.merges};
    }
}

/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_replay(device const ushort2 *pairs WARG(27),
    device WardWork &w WARG(30), device Params &p WARG(9),
    device State &s WARG(7))
{
    // The tail handles palettes of at most 256 entries, including errors.
    p.centers = 0;
    if (atomic_load_explicit(&s.failed, memory_order_relaxed)) {
        w.active = w.merges = 0;
        return;
    }
    for (uint step = 0; step < w.merges; ++step) {
        ushort2 pair = pairs[step];
        if (pair.x >= pair.y || pair.y >= w.initial ||
            !w.clusters[pair.x].weighted.w || !w.clusters[pair.y].weighted.w) {
            atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);
            w.active = w.merges = 0;
            return;
        }
        WardStats a = w.clusters[pair.x], b = w.clusters[pair.y];
        a.weighted += b.weighted;
        a.distinct += b.distinct;
        a.low = min(a.low, b.low);
        a.high = max(a.high, b.high);
        w.clusters[pair.x] = a;
        w.clusters[pair.y].weighted = uint4(0u);
    }
    uint dest = 0;
    for (uint slot = 0; slot < w.initial; ++slot) {
        WardStats a = w.clusters[slot];
        if (a.weighted.w)
            w.clusters[dest++] = a;
    }
    if (dest == w.active)
        p.centers = dest;
    else {
        atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);
        w.active = w.merges = 0;
    }
}

// A pair of unsigned words suffices for the exact numerator and cross
// products, including unit-weight placeholders for empty K-means centers.
struct WardWide { ulong low, high; };
static WardWide ward_multiply(ulong a, ulong b)
{
    ulong a0 = uint(a), a1 = a >> 32u, b0 = uint(b), b1 = b >> 32u;
    ulong first = a0 * b0;
    ulong mid = a1 * b0 + (first >> 32u);
    ulong tail = a0 * b1 + uint(mid);
    return { (tail << 32u) | uint(first), a1 * b1 + (mid >> 32u) +
        (tail >> 32u) };
}
static WardWide ward_add_wide(WardWide a, WardWide b)
{
    ulong low = a.low + b.low;
    return {low, a.high + b.high + ulong(low < a.low)};
}
static WardWide ward_distance_numerator(uint4 point, uint4 center, uint other)
{
    WardWide total = {0, 0};
    for (uint i = 0; i < 3; ++i) {
        long delta = long(point[i]) * center.w - long(center[i]) * point.w;
        ulong mag = ulong(delta < 0 ? -delta : delta);
        total = ward_add_wide(total, ward_multiply(mag, mag));
    }
    ulong scale = ulong(other) * other;
    WardWide out = ward_multiply(total.low, scale);
    out.high += total.high * scale;
    return out;
}
static bool ward_nearer(uint4 point, uint4 candidate, uint4 incumbent)
{
    WardWide a = ward_distance_numerator(point, candidate, incumbent.w);
    WardWide b = ward_distance_numerator(point, incumbent, candidate.w);
    return a.high < b.high || (a.high == b.high && a.low < b.low);
}

/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_lloyd_clear(device WardWork &w WARG(30),
    device float4 *means WARG(29), WID)
{
    if (w.merges && !w.stopped && i < w.active) {
        ward_clear(w.accum[i], w.use_stats);
        means[i] = float4(ward_mean(w.clusters[i].weighted), 0.0f);
    }
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_lloyd_assign(device WardWork &w WARG(30),
    device const float4 *means WARG(29),
    constant Geometry &geometry WARG(12), WID)
{
    if (!w.merges || w.stopped || i >= w.points_count)
        return;
    uint4 point = w.points[i];
    float3 value = ward_mean(point);
    // Heckbert resolves ties to the lowest index, while K-means retains
    // the previous owner if it is still a nearest center.
    uint owner = w.pass && !geometry.heckbert ? w.owner[i] : 0u;
    float3 delta = value - means[owner].xyz;
    float best = dot(delta, delta);
    for (uint k = 0; k < w.active; ++k) {
        if (k == owner)
            continue;
        delta = value - means[k].xyz;
        float d = dot(delta, delta);
        // A conservative bound for the FP32 error over [0,255] is less
        // than 0.0625 on the difference of two squared RGB distances.
        // Resolve all close decisions with the exact rational comparison.
        if (d < best - 0.0625f || (abs(d - best) <= 0.0625f &&
            ward_nearer(point, w.clusters[k].weighted,
                         w.clusters[owner].weighted))) {
            best = d;
            owner = k;
        }
    }
    w.owner[i] = owner;
    w.residual[i] = best * point.w;
    ward_accumulate(w.accum[owner], point, w.use_stats);
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_lloyd_update(device WardWork &w WARG(30),
    constant Geometry &geometry WARG(12))
{
    if (!w.merges || w.stopped)
        return;
    if (!geometry.heckbert) {
        for (uint k = 0; k < w.active; ++k) {
            if (atomic_load_explicit(&w.accum[k].weight[3],
                                     memory_order_relaxed))
                continue;
            uint donor = 0u;
            float largest = -1.0f;
            for (uint j = 0; j < w.points_count; ++j) {
                if (w.residual[j] > largest) {
                    donor = j;
                    largest = w.residual[j];
                }
            }
            if (largest < 0.0f)
                continue;
            uint old = w.owner[donor];
            for (uint c = 0; c < 4; ++c)
                atomic_fetch_sub_explicit(&w.accum[old].weight[c],
                    w.points[donor][c], memory_order_relaxed);
            ward_accumulate(w.accum[k], w.points[donor], false);
            w.owner[donor] = k;
            w.residual[donor] = 0.0f;
        }
    }
    float movement = 0.0f;
    bool changed = false;
    for (uint k = 0; k < w.active; ++k) {
        WardStats out = ward_load(w.accum[k], w.use_stats);
        if (!out.weighted.w) {
            w.clusters[k].distinct = uint4(0u);
            continue;
        }
        uint4 prior = w.clusters[k].weighted;
        long3 delta_numerator = long3(out.weighted.xyz) * long(prior.w) -
            long3(prior.xyz) * long(out.weighted.w);
        changed |= any(delta_numerator != long3(0));
        float3 delta = float3(delta_numerator) /
            (float(prior.w) * float(out.weighted.w));
        movement += dot(delta, delta);
        w.clusters[k] = out;
    }
    w.stopped = geometry.heckbert ? !changed : movement <= geometry.threshold;
    ++w.pass;
}
/* Accel: PALETTE-WARD-RESIDENT metal */
kernel void resident_ward_pack(device const WardWork &w WARG(30),
    device packed_uchar3 *pal WARG(21), constant Geometry &geometry WARG(12),
    constant uchar *tones WARG(20), WID)
{
    if (i >= w.active)
        return;
    WardStats a = w.clusters[i];
    uint4 color = a.weighted;
    if (a.distinct.w && geometry.representative == 1u)
        color = uint4(a.low.xyz + a.high.xyz, 2u);
    else if (a.distinct.w && geometry.representative == 2u)
        color = a.distinct;
    uint3 rounded = (2u * color.xyz + color.w) / (2u * color.w);
    if (geometry.heckbert)
        rounded = uint3(tones[rounded.x], tones[rounded.y], tones[rounded.z]);
    pal[i] = uchar3(rounded);
}
#undef WARG
#undef WID
