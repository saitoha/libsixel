// SPDX-License-Identifier: MIT
@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3u) {
    let s = diffuse_sample(id.x, id.y);
    var best = 0u;
    if (s.w > 0.0) { best = lookup_fractional(diffuse_query(s.xyz)); }
    diffuse_write(id.x, id.y, s, best);
}
