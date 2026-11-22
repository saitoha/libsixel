// SPDX-License-Identifier: MIT
@group(0) @binding(1) var<storage, read> palette: array<u32>;
// Each slot stores its complete cell key and a one-based palette index.
// Simultaneous misses and hash collisions only repeat a deterministic scan.
var<workgroup> cell_cache: array<atomic<u32>, 256>;

fn lookup(query: vec3i) -> u32 {
    let step = 1i << (8u - cell_bits);
    let last = (1i << cell_bits) - 1;
    let addr = min((query + step / 2) / step, vec3i(last));
    let key = (u32(addr.x) << (2u * cell_bits)) |
              (u32(addr.y) << cell_bits) | u32(addr.z);
    let slot = (key ^ (key >> 8u) ^ (key >> 16u)) & 255u;
    let hit = atomicLoad(&cell_cache[slot]);
    if (hit != 0u && (hit >> 9u) == key) { return (hit & 511u) - 1u; }

    // The first and last buckets are clipped by rounding and saturation.
    // Use twice the center to preserve half-byte centers exactly.
    let low = max(addr * step - step / 2, vec3i(0));
    let high = select((addr + 1) * step - step / 2 - 1, vec3i(255),
                      addr == vec3i(last));
    var best = 0u;
    var minimum = 2147483647i;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let p = palette[i];
        let rgb = vec3i(i32(p & 255u), i32((p >> 8u) & 255u), i32(p >> 16u));
        let delta = low + high - 2 * rgb;
        let d = dot(delta, delta);
        if (d < minimum) { minimum = d; best = i; }
    }
    atomicStore(&cell_cache[slot], (key << 9u) | (best + 1u));
    return best;
}

fn lookup_fractional(q: vec3f) -> u32 {
    // Every cell boundary is integral, including the first half-width cell.
    return lookup(vec3i(clamp(q, vec3f(0), vec3f(255))));
}
