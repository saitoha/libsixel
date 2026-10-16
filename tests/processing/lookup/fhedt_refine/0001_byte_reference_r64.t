#!/bin/sh
# Compare byte refinement with the ordered reference at R=64.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_refine/0001_byte_reference_r64 || {
    echo "not ok 1 - byte refinement reference at R=64"
    exit 0
}

echo "ok 1 - byte refinement reference at R=64"
exit 0
