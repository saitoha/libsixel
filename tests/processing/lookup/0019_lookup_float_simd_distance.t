#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0019_float_simd_distance" || {
    echo "not ok 1 - float simd distance"
    exit 0
}
echo "ok 1 - float simd distance"
exit 0
