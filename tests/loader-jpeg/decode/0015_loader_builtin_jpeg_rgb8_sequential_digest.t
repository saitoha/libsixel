#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Fix the byte-exact output of baseline sequential RGB JPEG decoding.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0108_loader_builtin_jpeg_rgb8_sequential_digest" || {
    printf '\n%s\n' "not ok 1 - JPEG sequential RGB8 exact digest"
    exit 0
}

printf '\n%s\n' "ok 1 - JPEG sequential RGB8 exact digest"
exit 0
