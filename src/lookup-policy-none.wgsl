// SPDX-License-Identifier: MIT
// Squared RGB8 distances fit in i32. A strict comparison gives ties to
// the lowest register, matching the CPU and Vulkan exhaustive policies.
@group(0) @binding(1) var<storage, read> palette: array<u32>;

fn lookup(query: vec3i) -> u32 {
    var best = 0u;
    var minimum = 2147483647i;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let word = palette[i];
        let rgb = vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                        i32(word >> 16u));
        let gap = query - rgb;
        let distance = dot(gap, gap);
        if (distance < minimum) {
            best = i;
            minimum = distance;
        }
    }
    return best;
}

fn lookup_fractional(q: vec3f) -> u32 {
    var best = 0u;
    var minimum = 1e30f;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let p = palette[i];
        let gap = q - vec3f(f32(p & 255u), f32((p >> 8u) & 255u), f32(p >> 16u));
        let d = dot(gap, gap);
        if (d < minimum) { minimum = d; best = i; }
    }
    return best;
}
