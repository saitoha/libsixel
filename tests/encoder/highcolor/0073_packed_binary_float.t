#!/bin/sh
# Explicit float promotes binary packed alpha.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0073_packed_binary_float || {
    echo "not ok 1 - high-color packed binary float"
    exit 0
}
echo "ok 1 - high-color packed binary float"
exit 0
