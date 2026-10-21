#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Sample 131072 RGB keys against a 300-color palette-order integer oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 300 0 131072 || {
    echo "not ok 1 - delaunay byte large palette SIMD indices"
    exit 0
}
echo "ok 1 - delaunay byte large palette SIMD indices"
exit 0
