#!/bin/sh
# Nearest resize preserves normalized colors and omissions.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0078_packed_resize || {
    echo "not ok 1 - high-color packed resize"
    exit 0
}
echo "ok 1 - high-color packed resize"
exit 0
