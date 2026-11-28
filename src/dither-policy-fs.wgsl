// SPDX-License-Identifier: MIT
// Accel: DITHER-FS webgpu
// Sender offsets and normalized error shares for fs.
const dx = array<i32, 4>(
    1, -1, 0, 1);
const dy = array<i32, 4>(
    0, 1, 1, 1);
const weights = array<f32, 4>(
    7.0 / 16.0, 3.0 / 16.0, 5.0 / 16.0, 1.0 / 16.0);
const tap_count = 4u;
