#!/bin/sh
# Verify public colorspace din99d inverse.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "8" || {
    echo "not ok 1 - public colorspace din99d inverse"
    exit 0
}
echo "ok 1 - public colorspace din99d inverse"
exit 0
