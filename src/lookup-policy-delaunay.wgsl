// SPDX-License-Identifier: MIT
// Same layout as Metal/Vulkan. Each row packs the retained neighbors in
// distance/index order. The degree excludes the padding in its final vector.
struct DelaunayBounds {
    low: vec3i,
    high: vec3i,
}

struct DelaunayTable {
    keys: array<array<u32, 256>, 256>,
    candidates: array<array<vec4u, 64>, 256>,
    degrees: array<u32, 256>,
    seeds: array<u32, 4096>,
    bounds: array<DelaunayBounds, 256>,
}

@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(8) var<storage, read_write> delaunay: DelaunayTable;

var<workgroup> delaunay_lows: array<vec3i, 32>;
var<workgroup> delaunay_highs: array<vec3i, 32>;
var<workgroup> delaunay_keys: array<u32, 256>;
var<workgroup> delaunay_totals: array<u32, 256>;
var<workgroup> delaunay_degree: u32;

fn delaunay_rgb(word: u32) -> vec3i {
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                 i32((word >> 16u) & 255u));
}

fn delaunay_distance(a: vec3i, b: vec3i) -> u32 {
    let gap = a - b;
    return u32(dot(gap, gap));
}

@compute @workgroup_size(32)
fn delaunay_bounds(@builtin(workgroup_id) group: vec3u,
                   @builtin(local_invocation_index) lane: u32) {
    let row = group.x;
    let point = delaunay_rgb(palette[row]);
    var lo = vec3i(0);
    var hi = vec3i(255);
    // One interval contraction of the original cube. Round outward in
    // signed integers to retain the continuous closed Voronoi cell.
    for (var k = lane; k < arrayLength(&palette); k += 32u) {
        let other = delaunay_rgb(palette[k]);
        let normal = 2 * (other - point);
        let limit = dot(other, other) - dot(point, point);
        for (var axis = 0u; axis < 3u; axis++) {
            let denominator = normal[axis];
            if (denominator == 0) { continue; }
            let u = (axis + 1u) % 3u;
            let v = (axis + 2u) % 3u;
            let numerator = limit - 255 * (min(normal[u], 0) + min(normal[v], 0));
            let quotient = numerator / denominator;
            let remainder = numerator - quotient * denominator;
            let round = i32(remainder > 0);
            if (denominator > 0) {
                hi[axis] = min(hi[axis], quotient + round);
            } else {
                lo[axis] = max(lo[axis], quotient - round);
            }
        }
    }
    delaunay_lows[lane] = lo;
    delaunay_highs[lane] = hi;
    workgroupBarrier();
    if (lane == 0u) {
        for (var k = 1u; k < 32u; k++) {
            lo = max(lo, delaunay_lows[k]);
            hi = min(hi, delaunay_highs[k]);
        }
        delaunay.bounds[row] = DelaunayBounds(lo, hi);
    }
}

fn delaunay_row_key(row: u32, lane: u32) -> u32 {
    // An empty row is never walked. Repeat the final neighbor in nonempty
    // tails so a four-wide query preserves the lowest-index tie rule.
    if (delaunay_degree == 0u) { return row; }
    return delaunay_keys[min(lane, delaunay_degree - 1u)];
}

fn delaunay_pack(key: u32) -> u32 {
    let index = key & 255u;
    return (palette[index] & 0xffffffu) | (index << 24u);
}

@compute @workgroup_size(256)
fn delaunay_sort(@builtin(workgroup_id) group: vec3u,
                 @builtin(local_invocation_index) lane: u32) {
    let row = group.x;
    var key = 0xffffffffu;
    var keep = false;
    if (lane < arrayLength(&palette) && lane != row) {
        let point = delaunay_rgb(palette[row]);
        let other = delaunay_rgb(palette[lane]);
        let lo = max(delaunay.bounds[row].low, delaunay.bounds[lane].low);
        let hi = min(delaunay.bounds[row].high, delaunay.bounds[lane].high);
        let normal = 2 * (other - point);
        let limit = dot(other, other) - dot(point, point);
        let lower = min(normal * lo, normal * hi);
        let upper = max(normal * lo, normal * hi);
        keep = all(lo <= hi) && lower.x + lower.y + lower.z <= limit &&
               upper.x + upper.y + upper.z >= limit;
        if (keep) { key = (delaunay_distance(point, other) << 8u) | lane; }
    }
    delaunay_keys[lane] = key;
    delaunay_totals[lane] = u32(keep);
    workgroupBarrier();
    if (lane == 0u) {
        var degree = 0u;
        for (var k = 0u; k < arrayLength(&palette); k++) {
            degree += delaunay_totals[k];
        }
        delaunay_degree = degree;
        delaunay.degrees[row] = degree;
    }
    // Sort the rejected pairs last, leaving a contiguous candidate row.
    for (var width = 2u; width <= 256u; width <<= 1u) {
        for (var step = width >> 1u; step > 0u; step >>= 1u) {
            let other = lane ^ step;
            if (other > lane) {
                let a = delaunay_keys[lane];
                let b = delaunay_keys[other];
                let ascending = (lane & width) == 0u;
                delaunay_keys[lane] = select(max(a, b), min(a, b), ascending);
                delaunay_keys[other] = select(min(a, b), max(a, b), ascending);
            }
            workgroupBarrier();
        }
    }
    key = delaunay_row_key(row, lane);
    delaunay.keys[row][lane] = key;
    // One lane owns each vector to avoid racing on its storage components.
    if (lane % 4u == 0u) {
        delaunay.candidates[row][lane / 4u] = vec4u(
            delaunay_pack(key),
            delaunay_pack(delaunay_row_key(row, lane + 1u)),
            delaunay_pack(delaunay_row_key(row, lane + 2u)),
            delaunay_pack(delaunay_row_key(row, lane + 3u)));
    }
}

@compute @workgroup_size(128)
fn delaunay_seeds(@builtin(global_invocation_id) id: vec3u) {
    let cell = id.x;
    let bucket = vec3u(cell >> 8u, (cell >> 4u) & 15u, cell & 15u);
    // Scale the rational cell centers by 32 for exact integer distances.
    let q = vec3i(2u * bucket + 1u) * 255;
    var best = 0u;
    var minimum = 0xffffffffu;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let distance = delaunay_distance(q, 32 * delaunay_rgb(palette[i]));
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    delaunay.seeds[cell] = best;
}

fn delaunay_seed(q: vec3u) -> u32 {
    let bucket = q >> vec3u(4u);
    return delaunay.seeds[(bucket.x << 8u) | (bucket.y << 4u) | bucket.z];
}

// Descend as soon as a four-neighbor block improves the packed key. The
// first distance in the next block bounds all remaining neighbors.
fn lookup(q: vec3i) -> u32 {
    var center = delaunay_seed(vec3u(q));
    var best = (delaunay_distance(q, delaunay_rgb(palette[center])) << 8u) | center;
    loop {
        let previous = best;
        let limit = 4u * (previous >> 8u);
        for (var at = 0u; at < delaunay.degrees[center]; at += 4u) {
            if ((delaunay.keys[center][at] >> 8u) > limit) { break; }
            let packed = delaunay.candidates[center][at / 4u];
            let mask = vec4u(255u);
            let dr = vec4i(packed & mask) - q.x;
            let dg = vec4i((packed >> vec4u(8u)) & mask) - q.y;
            let db = vec4i((packed >> vec4u(16u)) & mask) - q.z;
            let scores = (vec4u(dr * dr + dg * dg + db * db) << vec4u(8u)) |
                         (packed >> vec4u(24u));
            best = min(best, min(min(scores.x, scores.y), min(scores.z, scores.w)));
            if (best < previous) { break; }
        }
        if (best == previous) { return best & 255u; }
        center = best & 255u;
    }
}

fn lookup_fractional(q: vec3f) -> u32 {
    var center = delaunay_seed(vec3u(clamp(q, vec3f(0), vec3f(255))));
    let first = q - vec3f(delaunay_rgb(palette[center]));
    var minimum = dot(first, first);
    loop {
        var best = center;
        // One squared code-value unit of slack preserves the native
        // fractional radius test without altering candidate distances.
        let limit = 4.0 * minimum + 1.0;
        for (var at = 0u; at < delaunay.degrees[center]; at++) {
            if (f32(delaunay.keys[center][at] >> 8u) > limit) { break; }
            let packed = delaunay.candidates[center][at / 4u][at % 4u];
            let index = packed >> 24u;
            let gap = q - vec3f(delaunay_rgb(packed));
            let distance = dot(gap, gap);
            if (distance < minimum || (distance == minimum && index < best)) {
                minimum = distance;
                best = index;
                break;
            }
        }
        if (best == center) { return best; }
        center = best;
    }
}
