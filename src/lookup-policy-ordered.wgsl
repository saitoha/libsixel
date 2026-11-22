// SPDX-License-Identifier: MIT
@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(7) var<storage, read> ordered: array<u32>;

fn score(q: vec3i, i: u32) -> u32 {
    let p = palette[i];
    let d = q - vec3i(i32(p & 255u), i32((p >> 8u) & 255u), i32(p >> 16u));
    return (u32(dot(d, d)) << 8u) | i;
}

// A candidate closer to the query than the seed cannot lie farther than
// twice the query-to-seed distance from that seed. Include the boundary
// so equal-distance colors keep the smallest original palette index.
fn lookup(query: vec3i) -> u32 {
    let bucket = vec3u(query) >> vec3u(5);
    let cell = (bucket.x << 6u) | (bucket.y << 3u) | bucket.z;
    let seed = (ordered[cell / 4u] >> (8u * (cell % 4u))) & 255u;
    let n = arrayLength(&palette);
    let row = 128u + seed * n;
    var best = score(query, seed);
    let limit = 4u * (best >> 8u);
    for (var j = 0u; j < n; j++) {
        let key = ordered[row + j];
        if ((key >> 8u) > limit) { break; }
        best = min(best, score(query, key & 255u));
    }
    return best & 255u;
}

fn fractional_score(q: vec3f, i: u32) -> f32 {
    let p = palette[i];
    let gap = q - vec3f(f32(p & 255u), f32((p >> 8u) & 255u), f32(p >> 16u));
    return dot(gap, gap);
}

fn lookup_fractional(q: vec3f) -> u32 {
    let bucket = vec3u(clamp(q, vec3f(0), vec3f(255))) >> vec3u(5);
    let cell = (bucket.x << 6u) | (bucket.y << 3u) | bucket.z;
    var best = (ordered[cell / 4u] >> (8u * (cell % 4u))) & 255u;
    let n = arrayLength(&palette);
    let row = 128u + best * n;
    var minimum = fractional_score(q, best);
    let limit = 4.0 * minimum + 1.0;
    for (var j = 0u; j < n; j++) {
        let key = ordered[row + j];
        if (f32(key >> 8u) > limit) { break; }
        let i = key & 255u;
        let d = fractional_score(q, i);
        if (d < minimum || (d == minimum && i < best)) { best = i; minimum = d; }
    }
    return best;
}
