// SPDX-License-Identifier: MIT
// Accel: LOOKUP-VPTREE webgpu
// Same 640 KiB layout as Metal/Vulkan: distance rows, packed RGB rows,
// then the nearest register to each of 32^3 RGB cell centers.
struct VptreeTable {
    keys: array<array<u32, 256>, 256>,
    candidates: array<array<vec4u, 64>, 256>,
    seeds: array<u32, 32768>,
}

@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(8) var<storage, read_write> vptree: VptreeTable;

var<workgroup> vptree_keys: array<u32, 256>;
var<workgroup> vptree_own_at: u32;

fn vptree_rgb(word: u32) -> vec3i {
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                 i32((word >> 16u) & 255u));
}

fn vptree_distance(q: vec3i, i: u32) -> u32 {
    let gap = q - vptree_rgb(palette[i]);
    return u32(dot(gap, gap));
}

fn vptree_row_at(lane: u32, count: u32) -> u32 {
    // Omit the seed itself. Repeat the final candidate in short tails
    // so a four-color load always stays inside the selected row.
    let at = min(lane, max(count, 2u) - 2u);
    return at + u32(count > 1u && at >= vptree_own_at);
}

fn vptree_pack(key: u32) -> u32 {
    let index = key & 255u;
    return (palette[index] & 0xffffffu) | (index << 24u);
}

@compute @workgroup_size(256)
fn vptree_build_rows(@builtin(workgroup_id) group: vec3u,
                     @builtin(num_workgroups) groups: vec3u,
                     @builtin(local_invocation_index) lane: u32) {
    let row = group.x;
    // The planner dispatches one workgroup per palette entry. This count
    // is uniform across the sorting barriers.
    let count = groups.x;
    let center = vptree_rgb(palette[row]);
    var key = 0xffffffffu;
    if (lane < count) {
        key = (vptree_distance(center, lane) << 8u) | lane;
    }
    vptree_keys[lane] = key;
    workgroupBarrier();
    // Sort distance/index keys together so duplicate colors remain distinct.
    for (var width = 2u; width < 2u * count; width <<= 1u) {
        for (var step = width >> 1u; step > 0u; step >>= 1u) {
            let other = lane ^ step;
            if (other > lane) {
                let a = vptree_keys[lane];
                let b = vptree_keys[other];
                let ascending = (lane & width) == 0u;
                vptree_keys[lane] = select(max(a, b), min(a, b), ascending);
                vptree_keys[other] = select(min(a, b), max(a, b), ascending);
            }
            workgroupBarrier();
        }
    }
    if ((vptree_keys[lane] & 255u) == row) {
        vptree_own_at = lane;
    }
    workgroupBarrier();
    key = vptree_keys[vptree_row_at(lane, count)];
    vptree.keys[row][lane] = key;
    // A single lane writes each vector. Component stores by multiple lanes
    // could otherwise race on the same storage location.
    if (lane % 4u == 0u) {
        vptree.candidates[row][lane / 4u] = vec4u(
            vptree_pack(key),
            vptree_pack(vptree_keys[vptree_row_at(lane + 1u, count)]),
            vptree_pack(vptree_keys[vptree_row_at(lane + 2u, count)]),
            vptree_pack(vptree_keys[vptree_row_at(lane + 3u, count)]));
    }
}

@compute @workgroup_size(128)
fn vptree_build_seeds(@builtin(global_invocation_id) id: vec3u) {
    let cell = id.x;
    let bucket = vec3u(cell >> 10u, (cell >> 5u) & 31u, cell & 31u);
    // Scale the rational cell centers by 64 to compare exact integers.
    let q = vec3i(2u * bucket + 1u) * 255;
    var best = 0u;
    var minimum = 0xffffffffu;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let gap = q - 64 * vptree_rgb(palette[i]);
        let distance = u32(dot(gap, gap));
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    vptree.seeds[cell] = best;
}

fn vptree_seed(q: vec3u) -> u32 {
    let bucket = q >> vec3u(3u);
    let cell = (bucket.x << 10u) | (bucket.y << 5u) | bucket.z;
    return vptree.seeds[cell];
}

// Every competitor at least as good as the seed lies within twice the
// query-to-seed distance. Include the boundary for lowest-index ties.
fn lookup(query: vec3i) -> u32 {
    let seed = vptree_seed(vec3u(query));
    var best = (vptree_distance(query, seed) << 8u) | seed;
    let limit = 4u * (best >> 8u);
    for (var at = 0u; at + 1u < arrayLength(&palette); at += 4u) {
        if ((vptree.keys[seed][at] >> 8u) > limit) { break; }
        let packed = vptree.candidates[seed][at / 4u];
        let mask = vec4u(255u);
        let dr = vec4i(packed & mask) - query.x;
        let dg = vec4i((packed >> vec4u(8u)) & mask) - query.y;
        let db = vec4i((packed >> vec4u(16u)) & mask) - query.z;
        let scores = (vec4u(dr * dr + dg * dg + db * db) << vec4u(8u)) |
                     (packed >> vec4u(24u));
        best = min(best, min(min(scores.x, scores.y), min(scores.z, scores.w)));
    }
    return best & 255u;
}

fn lookup_fractional(q: vec3f) -> u32 {
    let seed = vptree_seed(vec3u(clamp(q, vec3f(0), vec3f(255))));
    var best = seed;
    let first = q - vec3f(vptree_rgb(palette[seed]));
    var minimum = dot(first, first);
    // Keep the native fractional path's rounding slack and tie order.
    let limit = 4.0 * minimum + 1.0;
    for (var at = 0u; at + 1u < arrayLength(&palette); at++) {
        if (f32(vptree.keys[seed][at] >> 8u) > limit) { break; }
        let packed = vptree.candidates[seed][at / 4u][at % 4u];
        let i = packed >> 24u;
        let gap = q - vec3f(vptree_rgb(packed));
        let d = dot(gap, gap);
        if (d < minimum || (d == minimum && i < best)) {
            best = i;
            minimum = d;
        }
    }
    return best;
}
