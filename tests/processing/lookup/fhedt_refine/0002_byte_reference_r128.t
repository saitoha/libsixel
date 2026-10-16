#!/bin/sh
# Compare byte refinement with the ordered reference at R=128.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_refine/0002_byte_reference_r128 || {
    echo "not ok 1 - byte refinement reference at R=128"
    exit 0
}

echo "ok 1 - byte refinement reference at R=128"
exit 0
