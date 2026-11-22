// SPDX-License-Identifier: MIT
// Same layout as Metal/Vulkan: packed member rows, assignments, counts,
// packed RGB box bounds, then 32^3 packed seeds and 32^3 cluster masks.
struct RbcTable {
    members: array<array<vec4u, 64>, 16>,
    assignments: array<u32, 256>,
    counts: array<u32, 16>,
    lows: array<u32, 16>,
    highs: array<u32, 16>,
    seeds: array<u32, 32768>,
    masks: array<u32, 32768>,
}

@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(8) var<storage, read_write> rbc: RbcTable;

fn rbc_pack(i: u32) -> u32 {
    return (palette[i] & 0xffffffu) | (i << 24u);
}

fn rbc_rgb(word: u32) -> vec3i {
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                 i32((word >> 16u) & 255u));
}

fn rbc_pack_rgb(rgb: vec3i) -> u32 {
    return u32(rgb.x | (rgb.y << 8u) | (rgb.z << 16u));
}

fn rbc_distance(a: vec3i, b: vec3i) -> u32 {
    let gap = a - b;
    return u32(dot(gap, gap));
}

@compute @workgroup_size(32)
fn rbc_assign(@builtin(global_invocation_id) id: vec3u) {
    let count = arrayLength(&palette);
    if (id.x >= count) { return; }
    let groups = min(count, 16u);
    let color = rbc_rgb(palette[id.x]);
    var best = 0xffffffffu;
    var owner = 0u;
    // The first of the evenly spaced pivots keeps equal distances.
    for (var j = 0u; j < groups; j++) {
        let pivot = j * count / groups;
        let score = rbc_distance(color, rbc_rgb(palette[pivot]));
        if (score < best) {
            best = score;
            owner = j;
        }
    }
    rbc.assignments[id.x] = owner;
}

@compute @workgroup_size(32)
fn rbc_pack_clusters(@builtin(workgroup_id) group: vec3u,
                     @builtin(local_invocation_index) lane: u32) {
    if (lane != 0u) { return; }
    let cluster = group.x;
    var lo = vec3i(255);
    var hi = vec3i(0);
    var members = 0u;
    var packed = 0u;
    // One lane owns each row and records members in palette order.
    for (var i = 0u; i < arrayLength(&palette); i++) {
        if (rbc.assignments[i] != cluster) { continue; }
        packed = rbc_pack(i);
        let color = rbc_rgb(packed);
        rbc.members[cluster][members / 4u][members % 4u] = packed;
        members++;
        lo = min(lo, color);
        hi = max(hi, color);
    }
    rbc.counts[cluster] = members;
    rbc.lows[cluster] = rbc_pack_rgb(lo);
    rbc.highs[cluster] = rbc_pack_rgb(hi);
    // Repeat the final valid member before any four-color load.
    let end = (members + 3u) & ~3u;
    for (var at = members; at < end; at++) {
        rbc.members[cluster][at / 4u][at % 4u] = packed;
    }
}

fn rbc_build_cell(cell: u32, upper_face: i32) {
    let bucket = vec3u(cell >> 10u, (cell >> 5u) & 31u, cell & 31u);
    // Scale the rational center by 64 to compare exact integers.
    let center = vec3i(2u * bucket + 1u) * 255;
    var best = 0u;
    var minimum = 0xffffffffu;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let distance = rbc_distance(center, 64 * rbc_rgb(palette[i]));
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    let seed = rbc_pack(best);
    rbc.seeds[cell] = seed;
    let lo = vec3i(bucket) * 8;
    // Fractional queries also cover the continuous upper face.
    let hi = min(lo + upper_face, vec3i(255));
    let color = rbc_rgb(seed);
    let corner = max(abs(lo - color), abs(hi - color));
    let limit = dot(corner, corner);
    var mask = 0u;
    // Keep any cluster whose box can be as close as the seed anywhere
    // in the cell, including equal-distance ties on the boundary.
    for (var j = 0u; j < min(arrayLength(&palette), 16u); j++) {
        if (rbc.counts[j] == 0u) { continue; }
        let low = rbc_rgb(rbc.lows[j]);
        let high = rbc_rgb(rbc.highs[j]);
        let gap = max(max(low - hi, lo - high), vec3i(0));
        if (dot(gap, gap) <= limit) { mask |= 1u << j; }
    }
    rbc.masks[cell] = mask;
}

@compute @workgroup_size(128)
fn rbc_seed(@builtin(global_invocation_id) id: vec3u) {
    rbc_build_cell(id.x, 7);
}

@compute @workgroup_size(128)
fn rbc_seed_fractional(@builtin(global_invocation_id) id: vec3u) {
    rbc_build_cell(id.x, 8);
}

fn rbc_cell(q: vec3u) -> u32 {
    let bucket = q >> vec3u(3u);
    return (bucket.x << 10u) | (bucket.y << 5u) | bucket.z;
}

fn lookup(q: vec3i) -> u32 {
    let cell = rbc_cell(vec3u(q));
    let seed = rbc.seeds[cell];
    var best = (rbc_distance(q, rbc_rgb(seed)) << 8u) | (seed >> 24u);
    var mask = rbc.masks[cell];
    while (mask != 0u) {
        let j = firstTrailingBit(mask);
        mask &= mask - 1u;
        let count = rbc.counts[j];
        if (count == 0u) { continue; }
        let gap = max(max(rbc_rgb(rbc.lows[j]) - q,
                          q - rbc_rgb(rbc.highs[j])), vec3i(0));
        // Prune strictly so the packed score can resolve index ties.
        if (u32(dot(gap, gap)) > (best >> 8u)) { continue; }
        for (var at = 0u; at < count; at += 4u) {
            let packed = rbc.members[j][at / 4u];
            let byte = vec4u(255u);
            let dr = vec4i(packed & byte) - q.x;
            let dg = vec4i((packed >> vec4u(8u)) & byte) - q.y;
            let db = vec4i((packed >> vec4u(16u)) & byte) - q.z;
            let scores = (vec4u(dr * dr + dg * dg + db * db) << vec4u(8u)) |
                         (packed >> vec4u(24u));
            best = min(best, min(min(scores.x, scores.y), min(scores.z, scores.w)));
        }
    }
    return best & 255u;
}

fn lookup_fractional(q: vec3f) -> u32 {
    let cell = rbc_cell(vec3u(clamp(q, vec3f(0), vec3f(255))));
    let seed = rbc.seeds[cell];
    let first = q - vec3f(rbc_rgb(seed));
    var best = seed >> 24u;
    var minimum = dot(first, first);
    var mask = rbc.masks[cell];
    while (mask != 0u) {
        let j = firstTrailingBit(mask);
        mask &= mask - 1u;
        let gap = max(max(vec3f(rbc_rgb(rbc.lows[j])) - q,
                          q - vec3f(rbc_rgb(rbc.highs[j]))), vec3f(0));
        // The native rounding slack applies only to the box test.
        if (dot(gap, gap) > minimum + 1.0) { continue; }
        for (var at = 0u; at < rbc.counts[j]; at++) {
            let packed = rbc.members[j][at / 4u][at % 4u];
            let index = packed >> 24u;
            let gap = q - vec3f(rbc_rgb(packed));
            let distance = dot(gap, gap);
            if (distance < minimum || (distance == minimum && index < best)) {
                minimum = distance;
                best = index;
            }
        }
    }
    return best;
}
