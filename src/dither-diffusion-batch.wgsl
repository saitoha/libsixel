// SPDX-License-Identifier: MIT
// Accel: DITHER-ATKINSON webgpu
// Accel: DITHER-BURKES webgpu
// Accel: DITHER-FS webgpu
// Accel: DITHER-JAJUNI webgpu
// Accel: DITHER-LSO2 webgpu
// Accel: DITHER-SIERRA1 webgpu
// Accel: DITHER-SIERRA2 webgpu
// Accel: DITHER-SIERRA3 webgpu
// Accel: DITHER-STUCKI webgpu
// A collective shares bounds over its spatial 8x8 tile. All lanes must
// reach its barriers, including lanes without a query in this phase.
fn batch_sample(x: u32, y: u32) -> vec4f {
    if (params.pass_id > 0u && params.pass_id < 9u &&
            phase(x, y) != (params.pass_id - 1u) % 4u) { return vec4f(-1); }
    return diffuse_sample(x, y);
}

@compute @workgroup_size(32)
fn main(@builtin(workgroup_id) group: vec3u, @builtin(local_invocation_index) lane: u32) {
    let x = group.x * 8u + lane % 8u;
    let y = group.y * 8u + lane / 8u;
    let samples = array<vec4f, 2>(batch_sample(x, y), batch_sample(x, y + 4u));
    var q: array<vec4f, 2>;
    for (var j = 0u; j < 2u; j++) {
        q[j] = vec4f(diffuse_query(samples[j].xyz), max(0.0, samples[j].w));
    }
    let hits = lookup_pair(q, lane);
    diffuse_write(x, y, samples[0], hits.x);
    diffuse_write(x, y + 4u, samples[1], hits.y);
}
