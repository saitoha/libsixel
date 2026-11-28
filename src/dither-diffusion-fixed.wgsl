// SPDX-License-Identifier: MIT
// Accel: DITHER-ATKINSON webgpu
// Accel: DITHER-BURKES webgpu
// Accel: DITHER-FS webgpu
// Accel: DITHER-JAJUNI webgpu
// Accel: DITHER-SIERRA1 webgpu
// Accel: DITHER-SIERRA2 webgpu
// Accel: DITHER-SIERRA3 webgpu
// Accel: DITHER-STUCKI webgpu
// Preserve the direction of the incoming correction when any component
// reaches a soft limit. WGSL round uses nearest-even at half integers.
fn candidate(x: i32, y: i32, source: vec3f) -> vec3f {
    var incoming = vec3f(0);
    for (var tap = 0u; tap < tap_count; tap++) {
        let sy = y - dy[tap];
        let sx = x - direction(sy) * dx[tap];
        incoming += sender(sx, sy).xyz * weights[tap];
    }
    if (dot(incoming, incoming) < params.deadzone * params.deadzone) {
        return source;
    }
    let value = source + incoming;
    if (params.hard != 0u) { return round(clamp(value, vec3f(0), vec3f(255))); }
    var scale = 1.0;
    for (var c = 0u; c < 3u; c++) {
        if (value[c] < params.lower) {
            scale = min(scale, (params.lower - source[c]) / incoming[c]);
        } else if (value[c] > params.upper) {
            scale = min(scale, (params.upper - source[c]) / incoming[c]);
        }
    }
    return round(source + incoming * scale);
}

fn diffuse_query(value: vec3f) -> vec3f { return clamp(value, vec3f(0), vec3f(255)); }
