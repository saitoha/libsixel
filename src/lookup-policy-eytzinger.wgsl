// SPDX-License-Identifier: MIT
@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(7) var<storage, read> sorted: array<u32>;

fn color(rank: u32) -> vec3i {
    let p = palette[sorted[rank]];
    return vec3i(i32(p & 255u), i32((p >> 8u) & 255u), i32(p >> 16u));
}

// The native byte policy visits the lower rank first at each step and
// retains the first visited register on a distance tie.
fn lookup(query: vec3i) -> u32 {
    let key = dot(query, vec3i(1));
    let count = arrayLength(&sorted);
    var low = 0u;
    var high = count;
    while (low < high) {
        let mid = low + (high - low) / 2u;
        if (dot(color(mid), vec3i(1)) < key) { low = mid + 1u; }
        else { high = mid; }
    }
    let rank = min(low, count - 1u);
    var best = sorted[rank];
    let gap = query - color(rank);
    var distance = dot(gap, gap);
    var left = true;
    var right = true;
    for (var step = 1u; step <= 6u; step++) {
        for (var side = 0u; side < 2u; side++) {
            let r = select(rank - step, rank + step, side == 1u);
            if (r >= count || !select(left, right, side == 1u)) { continue; }
            let d = query - color(r);
            let projection = dot(d, vec3i(1));
            if (projection * projection > distance) {
                if (side == 0u) { left = false; } else { right = false; }
            } else if (dot(d, d) < distance) {
                best = sorted[r];
                distance = dot(d, d);
            }
        }
    }
    return best;
}
