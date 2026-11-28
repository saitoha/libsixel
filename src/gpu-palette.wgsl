// SPDX-License-Identifier: MIT
// Accel: PALETTE-MAP webgpu
@group(0) @binding(0) var<storage, read> pixels: array<u32>;
struct Params {
    count: u32, first: u32, width: u32, strength: f32, save_query: u32,
    phase_x: u32, phase_y: u32, channel_rgb: u32, gradient_factor: f32,
    gradient_size: u32, gradient_width: u32, gradient_height: u32,
}
@group(0) @binding(3) var<uniform> params: Params;
@group(0) @binding(5) var<storage, read_write> queries: array<u32>;
@group(0) @binding(4) var<storage, read_write> invalid: atomic<u32>;

fn channel(offset: u32) -> i32 {
    return i32((pixels[offset / 4u] >> ((offset % 4u) * 8u)) & 255u);
}
