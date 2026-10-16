#!/bin/sh
# Compare float refinement with the ordered reference at R=256.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_refine/0006_float_reference_r256 || {
    echo "not ok 1 - float refinement reference at R=256"
    exit 0
}

echo "ok 1 - float refinement reference at R=256"
exit 0
