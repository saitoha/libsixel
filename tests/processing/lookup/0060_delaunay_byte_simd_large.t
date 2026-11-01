#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare simd lookup with an independent oracle for large.
set -eux

echo "1..1"
set -v

status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 300 0 sample simd || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP byte SIMD lookup is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - delaunay byte simd large indices"
    exit 0
}
echo "ok 1 - delaunay byte simd large indices"
exit 0
