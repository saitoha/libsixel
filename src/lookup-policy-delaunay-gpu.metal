/* Delaunay cache layout in uints: distance rows 0, RGB rows 65536,
 * degrees 131072, 16^3 seeds 131328, int4 cell bounds 135424.
 * Bounds and row constructors share tracked storage with the query. */
static int3 delaunay_color(constant uchar *palette, uint i)
{
    return int3(palette[i * 3u], palette[i * 3u + 1u],
                palette[i * 3u + 2u]);
}
static uint delaunay_distance(int3 a, int3 b)
{
    int3 d = a - b;
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}
/* Exact outward integer boxes contain the continuous closed Voronoi cell.
 * Integer division rounds away from the feasible interval. Every reduction
 * therefore retains the cell, including points on shared closed faces.
 * Coarse boxes add edges; they never remove required adjacency. */
kernel void sixel_gpu_delaunay_bounds(constant uchar *palette [[buffer(2)]],
            constant Params &params [[buffer(6)]],
            device uint *table [[buffer(15)]],
            uint row [[threadgroup_position_in_grid]],
            uint lane [[thread_index_in_threadgroup]])
{
    device int4 *bounds = (device int4 *)(table + 135424u);
    int3 lo, hi, nextlo, nexthi, point, other, normal;
    int limit, rest, numerator, denominator, quotient, remainder;
    uint pass, k, axis, u, v;

    lo = int3(0);
    hi = int3(255);
    point = delaunay_color(palette, row);
    for (pass = 0; pass < 1u; ++pass) {
        nextlo = lo;
        nexthi = hi;
        for (k = lane; k < params.ncolors; k += 32) {
            other = delaunay_color(palette, k);
            normal = 2 * (other - point);
            limit = int(delaunay_distance(other, int3(0)))
                - int(delaunay_distance(point, int3(0)));
            for (axis = 0; axis < 3; ++axis) {
                denominator = normal[axis];
                if (denominator == 0)
                    continue;
                u = (axis + 1) % 3;
                v = (axis + 2) % 3;
                rest = normal[u] * (normal[u] >= 0 ? lo[u] : hi[u])
                    + normal[v] * (normal[v] >= 0 ? lo[v] : hi[v]);
                numerator = limit - rest;
                quotient = numerator / denominator;
                remainder = numerator % denominator;
                if (denominator > 0) {
                    quotient += remainder > 0;
                    nexthi[axis] = min(nexthi[axis], quotient);
                } else {
                    quotient -= remainder > 0;
                    nextlo[axis] = max(nextlo[axis], quotient);
                }
            }
        }
        lo = int3(simd_max(nextlo.x), simd_max(nextlo.y),
                  simd_max(nextlo.z));
        hi = int3(simd_min(nexthi.x), simd_min(nexthi.y),
                  simd_min(nexthi.z));
    }
    if (lane == 0) {
        bounds[row * 2] = int4(lo, 0);
        bounds[row * 2 + 1] = int4(hi, 0);
    }
}

/* Each lane classifies one pair. Sorting rejected entries to the end also
 * compacts the retained adjacency and directly emits vector query rows.
 * No separate pair buffer or serial per-color repacking is needed. */
kernel void sixel_gpu_delaunay_sort(constant uchar *palette [[buffer(2)]],
           constant Params &params [[buffer(6)]],
           device uint *table [[buffer(15)]],
           uint row [[threadgroup_position_in_grid]],
           uint lane [[thread_index_in_threadgroup]],
           uint threads [[threads_per_threadgroup]])
{
    device const int4 *bounds =
        (device const int4 *)(table + 135424u);
    threadgroup uint keys[256];
    threadgroup uint totals[8];
    threadgroup uint count;
    int3 point, other_point, lo, hi, normal, lower, upper;
    int limit;
    uint width, step, other, a, b, key, k, rgb, keep, sum;
    bool ascending;

    keep = 0;
    key = UINT_MAX;
    if (lane < params.ncolors && lane != row) {
        point = delaunay_color(palette, row);
        other_point = delaunay_color(palette, lane);
        lo = max(bounds[row * 2].xyz, bounds[lane * 2].xyz);
        hi = min(bounds[row * 2 + 1].xyz, bounds[lane * 2 + 1].xyz);
        normal = 2 * (other_point - point);
        limit = int(delaunay_distance(other_point, int3(0)))
            - int(delaunay_distance(point, int3(0)));
        lower = min(normal * lo, normal * hi);
        upper = max(normal * lo, normal * hi);
        keep = all(lo <= hi)
            && lower.x + lower.y + lower.z <= limit
            && upper.x + upper.y + upper.z >= limit;
        if (keep)
            key = (delaunay_distance(point, other_point) << 8) | lane;
    }
    keys[lane] = key;
    sum = simd_sum(keep);
    if ((lane & 31) == 0)
        totals[lane >> 5] = sum;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lane == 0) {
        sum = 0;
        for (k = 0; k < threads / 32; ++k)
            sum += totals[k];
        count = sum;
        table[131072u + row] = sum;
    }
    for (width = 2; width <= threads; width <<= 1) {
        for (step = width >> 1; step > 0; step >>= 1) {
            other = lane ^ step;
            if (other > lane) {
                a = keys[lane];
                b = keys[other];
                ascending = (lane & width) == 0;
                keys[lane] = ascending ? min(a, b) : max(a, b);
                keys[other] = ascending ? max(a, b) : min(a, b);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
    }
    /* Empty rows are never read by the walk. Nonempty tails repeat a real
     * neighbor, preserving exact distances and lowest-index ties. */
    key = count ? keys[min(lane, count - 1)] : row;
    k = key & 255;
    rgb = uint(palette[k * 3u]) | (uint(palette[k * 3u + 1u]) << 8)
        | (uint(palette[k * 3u + 2u]) << 16) | (k << 24);
    table[row * 256u + lane] = key;
    table[65536u + row * 256u + lane] = rgb;
}
kernel void sixel_gpu_delaunay_seeds(
    constant uchar *palette [[buffer(2)]],
    constant Params& params [[buffer(6)]],
    device uint *table [[buffer(15)]],
    uint cell [[thread_position_in_grid]])
{
    int3 q, color, gap;
    uint best, minimum, distance, i;

    if (cell >= 4096u) {
        return;
    }
    /* Exact rational centers scaled by 32. The largest squared distance
     * is below 2^28, so even signed intermediate arithmetic is exact. */
    q = int3(2u * (cell >> 8) + 1u,
             2u * ((cell >> 4) & 15u) + 1u,
             2u * (cell & 15u) + 1u) * 255;
    best = 0u;
    minimum = 0xffffffffu;
    for (i = 0u; i < params.ncolors; ++i) {
        color = int3(palette[i * 3u], palette[i * 3u + 1u],
                     palette[i * 3u + 2u]) * 32;
        gap = q - color;
        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    table[131328u + cell] = best;
}
static uint delaunay_palette_lookup(constant uchar *palette,
    device const uint *table, uchar3 query)
{
    uint center, best, previous, at, key, cell;
    uint4 packed, scores;
    int4 dr, dg, db;

    cell = (uint(query.x >> 4) << 8) | (uint(query.y >> 4) << 4)
        | uint(query.z >> 4);
    center = table[131328u + cell];
    best = (palette_distance(palette, query, center) << 8) | center;
    /* Closed-cell adjacency retains an improving neighbor until the exact
     * winner is reached. Strict packed-key descent also resolves ties and
     * prevents cycles through duplicate coordinates. */
    for (;;) {
        previous = best;
        for (at = 0u; at < table[131072u + center]; at += 4u) {
            if ((table[center * 256u + at] >> 8) > 4u * (previous >> 8)) {
                break;
            }
            packed = ((device const uint4 *)table)[
                16384u + center * 64u + at / 4u];
            dr = int4(packed & 255u) - int(query.x);
            dg = int4((packed >> 8) & 255u) - int(query.y);
            db = int4((packed >> 16) & 255u) - int(query.z);
            scores = (uint4(dr * dr + dg * dg + db * db) << 8)
                | (packed >> 24);
            key = min(min(scores.x, scores.y), min(scores.z, scores.w));
            best = min(best, key);
            if (best < previous) {
                break;
            }
        }
        if (best == previous) {
            return best & 255u;
        }
        center = best & 255u;
    }
}
