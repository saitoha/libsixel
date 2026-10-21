#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# Exhaust all RGB keys against a palette-order integer oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/0021_delaunay_byte_exact 64 2 || {
    echo "not ok 1 - delaunay byte grid exhaustive indices"
    exit 0
}
echo "ok 1 - delaunay byte grid exhaustive indices"
exit 0
