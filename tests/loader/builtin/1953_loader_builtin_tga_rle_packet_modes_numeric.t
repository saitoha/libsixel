#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA raw and repeated RLE packets with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0085_loader_builtin_tga_rle_packet_modes_numeric" || {
    echo "not ok 1 - TGA raw and repeated RLE packets"
    exit 0
}

echo "ok 1 - TGA raw and repeated RLE packets"
exit 0
