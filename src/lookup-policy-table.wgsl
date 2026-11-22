// SPDX-License-Identifier: MIT
@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(8) var<storage, read_write> table: array<u32>;

fn table_rgb(word: u32) -> vec3i {
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u), i32(word >> 16u));
}

fn table_score(q: vec3f, i: u32) -> f32 {
    let d = q - vec3f(table_rgb(palette[i]));
    return dot(d, d);
}
