#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare map lookup with an independent oracle for large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 300 0 sample map || {
    echo "not ok 1 - delaunay byte map large indices"
    exit 0
}
echo "ok 1 - delaunay byte map large indices"
exit 0
