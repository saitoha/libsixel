// SPDX-License-Identifier: MIT
// Sender offsets and normalized error shares for atkinson.
const dx = array<i32, 6>(
    1, 2, -1, 0, 1, 0);
const dy = array<i32, 6>(
    0, 0, 1, 1, 1, 2);
const weights = array<f32, 6>(
    1.0 / 8.0, 1.0 / 8.0, 1.0 / 8.0, 1.0 / 8.0, 1.0 / 8.0, 1.0 / 8.0);
const tap_count = 6u;
