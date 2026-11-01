#!/bin/sh
# RGBA fractional composition survives a byte request.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0066_packed_rgba || {
    echo "not ok 1 - high-color packed rgba"
    exit 0
}
echo "ok 1 - high-color packed rgba"
exit 0
