// SPDX-License-Identifier: MIT
// Accel: DITHER-BLUENOISE webgpu
// The tile and optional gradient slice follow the same byte layout as Wasm.
@group(0) @binding(6) var<storage, read> noise: array<u32>;

fn noise_byte(i: u32) -> u32 {
    return (noise[i / 4u] >> (8u * (i % 4u))) & 255u;
}

fn sample_noise(x: u32, y: u32) -> f32 {
    let i = (y & 63u) * 64u + (x & 63u);
    let normalized = f32(noise_byte(i)) / 127.5 - 1.0;
    return (normalized + 1.0) * 0.5;
}

fn positional_query(source: vec3i, x: u32, y: u32, strength: f32) -> vec3i {
    var weight = 1.0;
    let local = y * params.width + x - params.first;
    if (local < params.gradient_size && x < params.gradient_width &&
        y < params.gradient_height && params.gradient_factor > 0.0) {
        let level = f32(noise_byte(4096u + local)) / 255.0;
        weight = 1.0 - pow(level, params.gradient_factor);
    }
    var q = source;
    for (var c = 0u; c < 3u; c++) {
        let channel = select(0u, c + 1u, params.channel_rgb != 0u);
        let xx = x + params.phase_x + 17u * channel;
        let yy = y + params.phase_y + 31u * channel;
        let v = sample_noise(xx, yy) + sample_noise(xx + 13u, yy + 29u) - 1.0;
        q[c] = clamp(source[c] + i32(v * strength * weight * 32.0), 0, 255);
    }
    return q;
}
