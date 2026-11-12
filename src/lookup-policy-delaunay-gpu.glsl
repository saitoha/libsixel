/* SPDX-License-Identifier: MIT */
/* Delaunay's private conservative adjacency, ported from its Metal byte
 * and fractional programs. No other lookup supplies construction or search.
 * Workspace in uint words: distance rows at 0, packed RGB rows at 65536,
 * degrees at 131072, 16^3 seeds at 131328, two padded int3 bounds per color
 * at 135424. Total: 137472 words. Builders publish bounds, then sorted
 * adjacency, then seeds before either query entry can read the table.
 *
 * vulkan-builders: delaunay_bounds delaunay_sort delaunay_seeds
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer DelaunayWorkspace {
    uint delaunay_table[];
};

ivec3 delaunay_color(uint i)
{
    return ivec3(lookup_palette[i * 3u], lookup_palette[i * 3u + 1u],
        lookup_palette[i * 3u + 2u]);
}

uint delaunay_distance(ivec3 a, ivec3 b)
{
    ivec3 d;

    d = a - b;
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}

#if defined(SIXEL_LOOKUP_BUILDER)
shared ivec3 delaunay_lows[32], delaunay_highs[32];
shared uint delaunay_keys[256], delaunay_totals[256];
shared uint delaunay_count;

/* Outward integer boxes contain continuous closed Voronoi cells. The
 * complete 32-thread reduction works even with narrower hardware subgroups.
 * Loose bounds only add edges; they cannot remove necessary adjacency. */
void delaunay_bounds()
{
    ivec3 lo, hi, nextlo, nexthi, point, other, normal;
    int limit, rest, numerator, denominator, quotient, remainder;
    uint row, lane, k, axis, u, v, offset;

    row = gl_WorkGroupID.x;
    lane = gl_LocalInvocationID.x;
    lo = ivec3(0);
    hi = ivec3(255);
    point = delaunay_color(row);
    nextlo = lo;
    nexthi = hi;
    /* Both Metal programs perform exactly one interval-tightening pass. */
    for (k = lane; k < lookup_count; k += 32u) {
        other = delaunay_color(k);
        normal = 2 * (other - point);
        limit = int(delaunay_distance(other, ivec3(0)))
            - int(delaunay_distance(point, ivec3(0)));
        for (axis = 0u; axis < 3u; ++axis) {
            denominator = normal[axis];
            if (denominator == 0) continue;
            u = (axis + 1u) % 3u;
            v = (axis + 2u) % 3u;
            rest = normal[u] * (normal[u] >= 0 ? lo[u] : hi[u])
                + normal[v] * (normal[v] >= 0 ? lo[v] : hi[v]);
            numerator = limit - rest;
            /* GLSL '%' is undefined for negative operands. Reproduce
             * Metal's truncation toward zero and signed remainder from
             * nonnegative division; all intermediate values fit int32. */
            quotient = abs(numerator) / abs(denominator);
            if ((numerator < 0) != (denominator < 0)) quotient = -quotient;
            remainder = numerator - quotient * denominator;
            if (denominator > 0) {
                quotient += remainder > 0 ? 1 : 0;
                nexthi[axis] = min(nexthi[axis], quotient);
            } else {
                quotient -= remainder > 0 ? 1 : 0;
                nextlo[axis] = max(nextlo[axis], quotient);
            }
        }
    }
    delaunay_lows[lane] = nextlo;
    delaunay_highs[lane] = nexthi;
    memoryBarrierShared();
    barrier();
    if (lane == 0u) {
        for (k = 1u; k < 32u; ++k) {
            delaunay_lows[0] = max(delaunay_lows[0], delaunay_lows[k]);
            delaunay_highs[0] = min(delaunay_highs[0], delaunay_highs[k]);
        }
        offset = 135424u + row * 8u;
        for (axis = 0u; axis < 3u; ++axis) {
            delaunay_table[offset + axis] = uint(delaunay_lows[0][axis]);
            delaunay_table[offset + 4u + axis] = uint(delaunay_highs[0][axis]);
        }
        delaunay_table[offset + 3u] = 0u;
        delaunay_table[offset + 7u] = 0u;
    }
}

/* Each lane classifies a pair. A bitonic sort puts rejected pairs last,
 * compacts retained adjacency and directly emits the vector query rows. */
void delaunay_sort()
{
    ivec3 point, other_point, lo, hi, normal, lower, upper;
    int limit;
    uint row, lane, threads, width, step, other, a, b, key, k, rgb, keep;
    uint offset, neighbor, axis;
    bool ascending;

    row = gl_WorkGroupID.x;
    lane = gl_LocalInvocationID.x;
    threads = gl_WorkGroupSize.x;
    keep = 0u;
    key = 0xffffffffu;
    if (lane < lookup_count && lane != row) {
        point = delaunay_color(row);
        other_point = delaunay_color(lane);
        offset = 135424u + row * 8u;
        neighbor = 135424u + lane * 8u;
        for (axis = 0u; axis < 3u; ++axis) {
            lo[axis] = max(int(delaunay_table[offset + axis]),
                int(delaunay_table[neighbor + axis]));
            hi[axis] = min(int(delaunay_table[offset + 4u + axis]),
                int(delaunay_table[neighbor + 4u + axis]));
        }
        normal = 2 * (other_point - point);
        limit = int(delaunay_distance(other_point, ivec3(0)))
            - int(delaunay_distance(point, ivec3(0)));
        lower = min(normal * lo, normal * hi);
        upper = max(normal * lo, normal * hi);
        keep = all(lessThanEqual(lo, hi))
            && lower.x + lower.y + lower.z <= limit
            && upper.x + upper.y + upper.z >= limit ? 1u : 0u;
        if (keep != 0u)
            key = (delaunay_distance(point, other_point) << 8) | lane;
    }
    delaunay_keys[lane] = key;
    delaunay_totals[lane] = keep;
    memoryBarrierShared();
    barrier();
    if (lane == 0u) {
        delaunay_count = 0u;
        for (k = 0u; k < threads; ++k) delaunay_count += delaunay_totals[k];
        delaunay_table[131072u + row] = delaunay_count;
    }
    for (width = 2u; width <= threads; width <<= 1) {
        for (step = width >> 1; step > 0u; step >>= 1) {
            other = lane ^ step;
            if (other > lane) {
                a = delaunay_keys[lane];
                b = delaunay_keys[other];
                ascending = (lane & width) == 0u;
                delaunay_keys[lane] = ascending ? min(a, b) : max(a, b);
                delaunay_keys[other] = ascending ? max(a, b) : min(a, b);
            }
            memoryBarrierShared();
            barrier();
        }
    }
    /* Empty rows are never walked. Repeat the final real neighbor in a
     * nonempty row's padding so every four-wide load preserves exact ties. */
    key = delaunay_count != 0u
        ? delaunay_keys[min(lane, delaunay_count - 1u)] : row;
    k = key & 255u;
    rgb = uint(lookup_palette[k * 3u])
        | (uint(lookup_palette[k * 3u + 1u]) << 8)
        | (uint(lookup_palette[k * 3u + 2u]) << 16) | (k << 24);
    delaunay_table[row * 256u + lane] = key;
    delaunay_table[65536u + row * 256u + lane] = rgb;
}

void delaunay_seeds()
{
    ivec3 q, color, gap;
    uint cell, best, minimum, distance, i;

    cell = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (cell >= 4096u) return;
    /* Exact rational centers scaled by 32. Signed squared distances stay
     * below 2^28; a seed is a starting vertex, never a quantized answer. */
    q = ivec3(2u * (cell >> 8) + 1u,
        2u * ((cell >> 4) & 15u) + 1u, 2u * (cell & 15u) + 1u) * 255;
    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < lookup_count; ++i) {
        color = delaunay_color(i) * 32;
        gap = q - color;
        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (distance < minimum) { minimum = distance; best = i; }
    }
    delaunay_table[131328u + cell] = best;
}
#endif

/* Byte lookup follows the legacy Metal four-neighbor walk. Packed keys
 * strictly decrease, including duplicate/equidistant palette-index ties. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint cell, center, best, previous, at, offset, key;
    uvec4 packed, scores;
    ivec4 dr, dg, db;

    cell = ((query.x >> 4) << 8) | ((query.y >> 4) << 4) | (query.z >> 4);
    center = delaunay_table[131328u + cell];
    best = (delaunay_distance(ivec3(query), delaunay_color(center)) << 8)
        | center;
    for (;;) {
        previous = best;
        for (at = 0u; at < delaunay_table[131072u + center]; at += 4u) {
            if ((delaunay_table[center * 256u + at] >> 8)
                    > 4u * (previous >> 8)) break;
            offset = 65536u + center * 256u + at;
            packed = uvec4(delaunay_table[offset], delaunay_table[offset + 1u],
                delaunay_table[offset + 2u], delaunay_table[offset + 3u]);
            dr = ivec4(packed & 255u) - int(query.x);
            dg = ivec4((packed >> 8) & 255u) - int(query.y);
            db = ivec4((packed >> 16) & 255u) - int(query.z);
            scores = (uvec4(dr * dr + dg * dg + db * db) << 8)
                | (packed >> 24);
            key = min(min(scores.x, scores.y), min(scores.z, scores.w));
            best = min(best, key);
            if (best < previous) break;
        }
        if (best == previous) return best & 255u;
        center = best & 255u;
    }
}

/* The same continuous-cell graph supports the original fractional query.
 * One squared-code unit of radius slack covers float rounding and can only
 * retain extra neighbors; it does not alter the final distance comparison. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uvec3 code;
    uint cell, center, best, at, packed, index;
    float minimum, distance, limit;
    vec3 gap;

    code = uvec3(clamp(query, 0.0, 255.0)) >> 4;
    cell = (code.x << 8) | (code.y << 4) | code.z;
    center = delaunay_table[131328u + cell];
    gap = query - vec3(delaunay_color(center));
    minimum = dot(gap, gap);
    for (;;) {
        best = center;
        limit = 4.0 * minimum + 1.0;
        for (at = 0u; at < delaunay_table[131072u + center]; ++at) {
            if (float(delaunay_table[center * 256u + at] >> 8) > limit) break;
            packed = delaunay_table[65536u + center * 256u + at];
            index = packed >> 24;
            gap = query - vec3(packed & 255u, (packed >> 8) & 255u,
                (packed >> 16) & 255u);
            distance = dot(gap, gap);
            if (distance < minimum ||
                    (distance == minimum && index < best)) {
                minimum = distance;
                best = index;
                break;
            }
        }
        if (best == center) return best;
        center = best;
    }
}
