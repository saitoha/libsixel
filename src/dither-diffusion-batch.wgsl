// SPDX-License-Identifier: MIT
@compute @workgroup_size(32)
fn main(@builtin(workgroup_id) group: vec3u, @builtin(local_invocation_index) lane: u32) {
    let x = group.x * 8u + lane % 8u;
    let y = group.y * 8u + lane / 8u;
    let samples = array<vec4f, 2>(diffuse_sample(x, y), diffuse_sample(x, y + 4u));
    var q: array<vec4f, 2>;
    for (var j = 0u; j < 2u; j++) {
        q[j] = vec4f(diffuse_query(samples[j].xyz), max(0.0, samples[j].w));
    }
    let hits = lookup_pair(q, lane);
    diffuse_write(x, y, samples[0], hits.x);
    diffuse_write(x, y + 4u, samples[1], hits.y);
}
