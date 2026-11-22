// SPDX-License-Identifier: MIT
@group(0) @binding(2) var<storage, read_write> indexes: array<atomic<u32>>;

// A chunk can start or end inside a row. Ignore out-of-chunk pixels while
// keeping every lane in the collective. Unaligned rows share output words.
@compute @workgroup_size(32)
fn main(@builtin(workgroup_id) group: vec3u, @builtin(num_workgroups) groups: vec3u,
        @builtin(local_invocation_index) lane: u32) {
    let tile = group.x + groups.x * group.y;
    let columns = (params.width + 7u) / 8u;
    let x = (tile % columns) * 8u + lane % 8u;
    let y0 = params.first / params.width + (tile / columns) * 8u + lane / 8u;
    var q: array<vec4f, 2>;
    var at: array<u32, 2>;
    for (var j = 0u; j < 2u; j++) {
        let y = y0 + j * 4u;
        let pixel = y * params.width + x - params.first;
        at[j] = pixel;
        if (x >= params.width || pixel >= params.count) { continue; }
        let o = pixel * 3u;
        let src = vec3i(channel(o), channel(o + 1u), channel(o + 2u));
        let rgb = positional_query(src, x, y, params.strength);
        q[j] = vec4f(vec3f(rgb), 1);
        if (params.save_query != 0u) {
            queries[pixel] = u32(rgb.x) | (u32(rgb.y) << 8u) | (u32(rgb.z) << 16u);
        }
    }
    let hits = lookup_pair(q, lane);
    for (var j = 0u; j < 2u; j++) {
        if (q[j].w == 0.0) { continue; }
        if (hits[j] >= arrayLength(&palette)) { atomicStore(&invalid, 1u); }
        atomicOr(&indexes[at[j] / 4u], hits[j] << (8u * (at[j] % 4u)));
    }
}
