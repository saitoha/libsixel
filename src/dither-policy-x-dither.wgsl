// SPDX-License-Identifier: MIT
fn positional_query(source: vec3i, x: u32, y: u32, strength: f32) -> vec3i {
    var query = source;
    for (var c = 0u; c < 3u; c++) {
        let phase = ((x + c * 29u) ^ (y * 149u)) * 1234u;
        let noise = f32(phase & 511u) / 256.0 - 1.0;
        query[c] = clamp(source[c] + i32((noise * strength) * 32.0), 0, 255);
    }
    return query;
}
