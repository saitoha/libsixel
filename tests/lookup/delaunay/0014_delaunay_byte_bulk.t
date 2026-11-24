#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Compare cold and warm bulk rows with an independent integer oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/0034_delaunay_byte_bulk || {
    printf '\n%s\n' "not ok 1 - delaunay byte bulk exact indices and output bounds"
    exit 0
}
printf '\n%s\n' "ok 1 - delaunay byte bulk exact indices and output bounds"
exit 0
