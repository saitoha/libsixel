// SPDX-License-Identifier: MIT
@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3u, @builtin(workgroup_id) group: vec3u,
        @builtin(num_workgroups) grid: vec3u, @builtin(local_invocation_index) lane: u32) {
    var xy = id.xy;
    if (params.pass_id >= 1u && params.pass_id <= 8u) {
        // Sixteen workgroups exhaust one phase of a repeating 64x64 tile.
        let ordinal = group.y * grid.x + group.x;
        let tile = ordinal / 16u;
        let columns = (params.width + 63u) / 64u;
        let bucket = (params.pass_id - 1u) % 4u;
        let pos = tables[arrayLength(&tables) - 4096u + bucket * 1024u +
                         (ordinal % 16u) * 64u + lane];
        xy = 64u * vec2u(tile % columns, tile / columns) + vec2u(pos % 64u, pos / 64u);
    }
    let s = diffuse_sample(xy.x, xy.y);
    var best = 0u;
    if (s.w > 0.0) { best = lookup_fractional(diffuse_query(s.xyz)); }
    diffuse_write(xy.x, xy.y, s, best);
}
