/* SPDX-License-Identifier: MIT */
/* Accel: PALETTE-WARD-RESIDENT vulkan
 * vulkan-stages: ward_km_points ward_km_init ward_km_collect ward_km_clusters
 * vulkan-stages: ward_hb_points ward_hb_clusters ward_seed ward_replay
 * vulkan-stages: ward_lloyd_clear ward_lloyd_assign ward_lloyd_update ward_pack
 */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(local_size_x_id = 0) in;
layout(std430, binding = 0) readonly buffer Geometry {
    uint width, height, stride, samples, colors, min_iterations;
    float threshold;
    uint cover, soft, grow;
    uint snap[256], tones[256], anchors[26];
    uint target, heckbert, representative;
} g;
layout(std430, binding = 1) readonly buffer Points { float input_points[]; };
layout(std430, binding = 2) buffer Params { uint points, centers; } p;
layout(std430, binding = 3) readonly buffer HbParams { uint count, colors; } hb;
layout(std430, binding = 4) readonly buffer Order { uint order[]; };
struct Box { uint start, size, sum; float priority; };
layout(std430, binding = 5) readonly buffer Boxes { Box boxes[]; };
layout(std430, binding = 6) readonly buffer Solved { uint solved; };
layout(std430, binding = 7) readonly buffer Histogram { uint histogram[]; };
layout(std430, binding = 8) buffer Result {
    uint final_count, failed;
    uint8_t palette[];
};
layout(std430, binding = 9) buffer State { uint done, iters, lloyd_failed; };
layout(std430, binding = 10) readonly buffer Membership { uint member[]; };
layout(std430, binding = 11) readonly buffer Centers { float seed_centers[]; };

const uint WARD_POINTS = 32768u;
const uint WARD_SLOTS = 768u;
struct Stats { uvec4 weighted, distinct, low, high; };
/* Byte RGB sums and integer masses remain exact through all merges. */
layout(std430, binding = 12) buffer WardWork {
    uvec4 points[WARD_POINTS];
    uint owner[WARD_POINTS];
    float residual[WARD_POINTS];
    Stats clusters[WARD_SLOTS], accum[WARD_SLOTS];
    uint points_count, initial, active_count, pass, stopped, merges, use_stats;
} w;
layout(std430, binding = 13) buffer Means { vec4 means[]; };
layout(std430, binding = 14) buffer WardParams { uint count, merges; } ward;
layout(std430, binding = 15) readonly buffer Pairs { uint pairs[]; };

Stats empty_stats()
{
    return Stats(uvec4(0), uvec4(0), uvec4(255), uvec4(0));
}

vec3 mean_color(uvec4 p)
{
    return vec3(p.xyz) / float(p.w);
}

Stats add_stats(Stats a, uvec4 b)
{
    uvec3 color = b.xyz / b.w;
    a.weighted += b;
    a.distinct += uvec4(color, 1u);
    a.low.xyz = min(a.low.xyz, color);
    a.high.xyz = max(a.high.xyz, color);
    return a;
}

void accumulate(uint k, uvec4 b, bool stats)
{
    for (uint c = 0u; c < 4u; ++c)
        atomicAdd(w.accum[k].weighted[c], b[c]);
    if (stats) {
        uvec3 color = b.xyz / b.w;
        for (uint c = 0u; c < 3u; ++c) {
            atomicAdd(w.accum[k].distinct[c], color[c]);
            atomicMin(w.accum[k].low[c], color[c]);
            atomicMax(w.accum[k].high[c], color[c]);
        }
        atomicAdd(w.accum[k].distinct.w, 1u);
    }
}

void ward_km_points()
{
    uint i = gl_GlobalInvocationID.x;
    uint out_at = histogram[WARD_POINTS * 4u + i];
    for (uint j = i * 128u; j < (i + 1u) * 128u; ++j) {
        uint k = j * 4u;
        uvec4 cell = uvec4(histogram[k], histogram[k + 1u],
            histogram[k + 2u], histogram[k + 3u]);
        if (cell.w != 0u)
            w.points[out_at++] = cell;
    }
}

void ward_km_init()
{
    uint i = gl_GlobalInvocationID.x;
    if (i < g.colors)
        w.accum[i].weighted = uvec4(0);
    if (i == 0u) {
        w.points_count = p.points;
        w.initial = p.centers;
        w.use_stats = 0u;
        w.stopped = w.pass = 0u;
        failed = lloyd_failed | uint(iters == 0u);
        // Refresh memberships against the final oversplit centers.
        done = failed;
    }
}

void ward_km_collect()
{
    uint i = gl_GlobalInvocationID.x;
    if (failed == 0u && i < w.points_count)
        accumulate(member[i], w.points[i], false);
}

void ward_km_clusters()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= g.colors)
        return;
    Stats s = empty_stats();
    if (i < w.initial) {
        if (w.initial <= g.target) {
            uint j = 3u * i;
            s.weighted = uvec4(uvec3(seed_centers[j], seed_centers[j + 1u],
                seed_centers[j + 2u]), 1u);
        } else {
            s.weighted = w.accum[i].weighted;
            s.weighted.w = max(s.weighted.w, 1u);
        }
    }
    w.clusters[i] = s;
}

void ward_hb_points()
{
    uint i = gl_GlobalInvocationID.x;
    if (i < hb.count) {
        uint j = 4u * i, mass = uint(input_points[j + 3u]);
        uvec3 color = uvec3(input_points[j], input_points[j + 1u],
                           input_points[j + 2u]);
        w.points[i] = uvec4(color * mass, mass);
    }
}

void ward_hb_clusters()
{
    uint i = gl_GlobalInvocationID.x, count = min(hb.count, hb.colors);
    if (i == 0u) {
        w.points_count = hb.count;
        w.initial = count;
        w.use_stats = uint(g.representative != 0u);
        w.stopped = w.pass = 0u;
    }
    if (i >= count)
        return;
    if (hb.count <= hb.colors) {
        w.clusters[i] = add_stats(empty_stats(), w.points[i]);
        return;
    }
    Box b = boxes[i];
    if (solved != count || b.size == 0u || b.start >= hb.count ||
            b.size > hb.count - b.start ||
            isnan(b.priority) || isinf(b.priority)) {
        atomicOr(failed, 1u);
        return;
    }
    uint rank = 0u;
    for (uint j = 0u; j < count; ++j) {
        Box other = boxes[j];
        rank += uint(other.priority > b.priority ||
            (other.priority == b.priority && other.start < b.start));
    }
    Stats s = empty_stats();
    for (uint j = b.start; j < b.start + b.size; ++j) {
        if (order[j] >= hb.count) {
            atomicOr(failed, 1u);
            return;
        }
        s = add_stats(s, w.points[order[j]]);
    }
    if (s.weighted.w == 0u || s.weighted.w != b.sum) {
        atomicOr(failed, 1u);
        return;
    }
    w.clusters[rank] = s;
}

void ward_seed()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= g.colors)
        return;
    uvec4 s = failed == 0u && i < w.initial
        ? w.clusters[i].weighted : uvec4(0);
    means[i] = s.w != 0u ? vec4(mean_color(s), s.w) : vec4(0);
    if (i == 0u) {
        w.active_count = failed != 0u ? 0u : min(w.initial, g.target);
        w.merges = failed != 0u ? 0u : w.initial - w.active_count;
        ward.count = g.colors;
        ward.merges = w.merges;
    }
}

void ward_replay()
{
    p.centers = 0u;
    for (uint i = 0u; i < w.merges; ++i) {
        uint kept = pairs[i] >> 16u, removed = pairs[i] & 0xffffu;
        if (kept >= removed || removed >= w.initial ||
                w.clusters[kept].weighted.w == 0u ||
                w.clusters[removed].weighted.w == 0u) {
            failed = 1u;
            w.active_count = w.merges = 0u;
            return;
        }
        Stats a = w.clusters[kept], b = w.clusters[removed];
        a.weighted += b.weighted;
        a.distinct += b.distinct;
        a.low = min(a.low, b.low);
        a.high = max(a.high, b.high);
        w.clusters[kept] = a;
        w.clusters[removed].weighted = uvec4(0);
    }
    uint dest = 0u;
    if (failed == 0u) {
        for (uint i = 0u; i < w.initial; ++i) {
            Stats a = w.clusters[i];
            if (a.weighted.w != 0u)
                w.clusters[dest++] = a;
        }
    }
    if (dest == w.active_count)
        p.centers = dest;
    else {
        failed = 1u;
        w.active_count = w.merges = 0u;
    }
}

/* Two words cover all squared numerators and cross products at the 32768
 * sample limit, including K-means' empty-cluster unit-weight placeholders. */
struct Wide { uint64_t low, high; };
Wide multiply_wide(uint64_t a, uint64_t b)
{
    uint64_t a0 = uint(a), a1 = a >> 32u, b0 = uint(b), b1 = b >> 32u;
    uint64_t first = a0 * b0;
    uint64_t mid = a1 * b0 + (first >> 32u);
    uint64_t tail = a0 * b1 + uint(mid);
    return Wide((tail << 32u) | uint(first),
        a1 * b1 + (mid >> 32u) + (tail >> 32u));
}

Wide add_wide(Wide a, Wide b)
{
    uint64_t low = a.low + b.low;
    return Wide(low, a.high + b.high + uint(low < a.low));
}

Wide distance_numerator(uvec4 point, uvec4 center, uint other)
{
    Wide total = Wide(0ul, 0ul);
    for (uint c = 0u; c < 3u; ++c) {
        int64_t diff = int64_t(point[c]) * center.w -
            int64_t(center[c]) * point.w;
        uint64_t mag = uint64_t(abs(diff));
        total = add_wide(total, multiply_wide(mag, mag));
    }
    uint64_t scale = uint64_t(other) * other;
    Wide value = multiply_wide(total.low, scale);
    value.high += total.high * scale;
    return value;
}

bool nearer(uvec4 point, uvec4 candidate, uvec4 incumbent)
{
    Wide a = distance_numerator(point, candidate, incumbent.w);
    Wide b = distance_numerator(point, incumbent, candidate.w);
    return a.high < b.high || (a.high == b.high && a.low < b.low);
}

void ward_lloyd_clear()
{
    uint i = gl_GlobalInvocationID.x;
    if (w.merges != 0u && w.stopped == 0u && i < w.active_count) {
        w.accum[i] = empty_stats();
        means[i] = vec4(mean_color(w.clusters[i].weighted), 0.0);
    }
}

void ward_lloyd_assign()
{
    uint i = gl_GlobalInvocationID.x;
    if (w.merges == 0u || w.stopped != 0u || i >= w.points_count)
        return;
    uvec4 pt = w.points[i];
    vec3 value = mean_color(pt);
    // Preserve the original owner on a K-means tie, and the first on Heckbert.
    uint owner = w.pass != 0u && g.heckbert == 0u ? w.owner[i] : 0u;
    vec3 diff = value - means[owner].xyz;
    float best = dot(diff, diff);
    for (uint k = 0u; k < w.active_count; ++k) {
        if (k == owner)
            continue;
        diff = value - means[k].xyz;
        float d = dot(diff, diff);
        // Differences outside this margin are stable in FP32 on [0,255].
        if (d < best - 0.0625 || (abs(d - best) <= 0.0625 &&
            nearer(pt, w.clusters[k].weighted, w.clusters[owner].weighted))) {
            best = d;
            owner = k;
        }
    }
    w.owner[i] = owner;
    w.residual[i] = best * pt.w;
    accumulate(owner, pt, w.use_stats != 0u);
}

void ward_lloyd_update()
{
    if (w.merges == 0u || w.stopped != 0u)
        return;
    if (g.heckbert == 0u) {
        for (uint k = 0u; k < w.active_count; ++k) {
            if (w.accum[k].weighted.w != 0u)
                continue;
            uint donor = 0u;
            float largest = -1.0;
            for (uint j = 0u; j < w.points_count; ++j) {
                if (w.residual[j] > largest) {
                    donor = j;
                    largest = w.residual[j];
                }
            }
            w.accum[w.owner[donor]].weighted -= w.points[donor];
            w.accum[k].weighted = w.points[donor];
            w.owner[donor] = k;
            w.residual[donor] = 0.0;
        }
    }
    float movement = 0.0;
    bool changed = false;
    for (uint k = 0u; k < w.active_count; ++k) {
        Stats s = w.accum[k];
        if (s.weighted.w == 0u) {
            w.clusters[k].distinct = uvec4(0);
            continue;
        }
        uvec4 prior = w.clusters[k].weighted;
        i64vec3 diff = i64vec3(s.weighted.xyz) * int64_t(prior.w) -
            i64vec3(prior.xyz) * int64_t(s.weighted.w);
        changed = changed || any(notEqual(diff, i64vec3(0)));
        vec3 step = vec3(diff) / (float(prior.w) * float(s.weighted.w));
        movement += dot(step, step);
        w.clusters[k] = s;
    }
    w.stopped = uint(g.heckbert != 0u ? !changed : movement <= g.threshold);
    ++w.pass;
}

void ward_pack()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= w.active_count)
        return;
    Stats s = w.clusters[i];
    uvec4 color = s.weighted;
    if (s.distinct.w != 0u && g.representative == 1u)
        color = uvec4(s.low.xyz + s.high.xyz, 2u);
    else if (s.distinct.w != 0u && g.representative == 2u)
        color = s.distinct;
    uvec3 rgb = (2u * color.xyz + color.w) / (2u * color.w);
    if (g.heckbert != 0u)
        rgb = uvec3(g.tones[rgb.x], g.tones[rgb.y], g.tones[rgb.z]);
    for (uint c = 0u; c < 3u; ++c)
        palette[3u * i + c] = uint8_t(rgb[c]);
}
