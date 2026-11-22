// SPDX-License-Identifier: MIT
const cell_shift = 8u - cell_bits;
const cell_step = 1i << cell_shift;
const cell_last = (1u << cell_bits) - 1u;

var<workgroup> cell_indexes: array<u32, 64>;

@compute @workgroup_size(64)
fn cell_build(@builtin(global_invocation_id) id: vec3u,
              @builtin(local_invocation_index) lane: u32) {
    let cell = id.x;
    let address = vec3u(cell >> (2u * cell_bits), (cell >> cell_bits) & cell_last,
                        cell & cell_last);
    // Rounding and saturation clip the first and last buckets. Twice the
    // center keeps half-byte centers exact, including at both boundaries.
    let addr = vec3i(address);
    let low = max(addr * cell_step - cell_step / 2, vec3i(0));
    let high = select((addr + 1) * cell_step - cell_step / 2 - 1, vec3i(255),
                      address == vec3u(cell_last));
    let center2 = low + high;
    var best = 0u;
    var minimum = 2147483647i;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let delta = center2 - 2 * table_rgb(palette[i]);
        let d = dot(delta, delta);
        if (d < minimum) { minimum = d; best = i; }
    }
    // Both table sizes are multiples of 64. Each fourth lane owns a
    // complete storage word, so neighboring byte writes cannot race.
    cell_indexes[lane] = best;
    workgroupBarrier();
    if (lane % 4u == 0u) {
        table[cell / 4u] = best | (cell_indexes[lane + 1u] << 8u) |
            (cell_indexes[lane + 2u] << 16u) | (cell_indexes[lane + 3u] << 24u);
    }
}

fn lookup(query: vec3i) -> u32 {
    let addr = min(vec3u(query + cell_step / 2) >> vec3u(cell_shift), vec3u(cell_last));
    let cell = (addr.x << (2u * cell_bits)) | (addr.y << cell_bits) | addr.z;
    return (table[cell / 4u] >> (8u * (cell % 4u))) & 255u;
}

fn lookup_fractional(q: vec3f) -> u32 {
    // Every cell boundary is integral, including the first half-width cell.
    return lookup(vec3i(clamp(q, vec3f(0), vec3f(255))));
}
