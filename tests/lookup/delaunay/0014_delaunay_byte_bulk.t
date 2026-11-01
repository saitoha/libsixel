#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare cold and warm bulk rows with an independent integer oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/0034_delaunay_byte_bulk || {
    echo "not ok 1 - delaunay byte bulk exact indices and output bounds"
    exit 0
}
echo "ok 1 - delaunay byte bulk exact indices and output bounds"
exit 0
