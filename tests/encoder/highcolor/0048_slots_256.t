#!/bin/sh
# The 256th key is painted by a pending final pass.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0048_slots_256 || {
    echo "not ok 1 - high-color slots 256"
    exit 0
}
echo "ok 1 - high-color slots 256"
exit 0
