#!/bin/sh
# Ignored packed alpha retains an independent side mask.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0074_packed_alpha_disabled || {
    echo "not ok 1 - high-color packed alpha disabled"
    exit 0
}
echo "ok 1 - high-color packed alpha disabled"
exit 0
