// SPDX-License-Identifier: MIT
// Accel: DITHER-SIERRA1 webgpu
// Sender offsets and normalized error shares for sierra1.
const dx = array<i32, 3>(
    1, -1, 0);
const dy = array<i32, 3>(
    0, 1, 1);
const weights = array<f32, 3>(
    2.0 / 4.0, 1.0 / 4.0, 1.0 / 4.0);
const tap_count = 3u;
