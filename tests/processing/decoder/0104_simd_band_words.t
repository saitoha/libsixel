#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Compare the vector band-word helpers with their scalar updates.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0104_simd_band_words" || {
    echo "not ok 1 - SIMD band words match scalar updates"
    exit 0
}
echo "ok 1 - SIMD band words match scalar updates"
exit 0
