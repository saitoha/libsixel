// SPDX-License-Identifier: MIT
// Accel: DITHER-STUCKI webgpu
// Sender offsets and normalized error shares for stucki.
const dx = array<i32, 12>(
    1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2);
const dy = array<i32, 12>(
    0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2);
const weights = array<f32, 12>(
    8.0 / 42.0, 4.0 / 42.0, 2.0 / 42.0, 4.0 / 42.0,
    8.0 / 42.0, 4.0 / 42.0, 2.0 / 42.0, 1.0 / 42.0,
    2.0 / 42.0, 4.0 / 42.0, 2.0 / 42.0, 1.0 / 42.0);
const tap_count = 12u;
