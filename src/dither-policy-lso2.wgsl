// SPDX-License-Identifier: MIT
// Accel: DITHER-LSO2 webgpu
// Each channel's error magnitude selects its own calibrated coefficient.
fn weighted(x: i32, y: i32, tap: u32) -> vec3f {
    let e = sender(x, y).xyz;
    let row = vec3u(clamp(floor(abs(e) + 0.5), vec3f(0), vec3f(255)));
    let base = 1024u + tap;
    return e * vec3f(bitcast<f32>(tables[base + 8u * row.x]),
                     bitcast<f32>(tables[base + 8u * row.y]),
                     bitcast<f32>(tables[base + 8u * row.z]));
}

fn candidate(x: i32, y: i32, source: vec3f) -> vec3f {
    let d = direction(y);
    let above = direction(y - 1);
    var e = weighted(x - d, y, 0u);
    e += weighted(x - 4 * d, y, 1u);
    e += weighted(x + above, y - 1, 2u);
    e += weighted(x, y - 1, 3u);
    e += weighted(x - above, y - 1, 4u);
    e += weighted(x, y - 4, 5u);
    let old = sender(x, y);
    let prior = old.xyz + diffuse_color(u32(old.w));
    return prior + (clamp(source + e, vec3f(0), vec3f(255)) - prior);
}

fn diffuse_query(value: vec3f) -> vec3f { return value; }
