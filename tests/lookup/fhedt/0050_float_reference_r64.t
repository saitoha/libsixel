#!/bin/sh
# Compare float refinement with the ordered reference at R=64.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_refine/0004_float_reference_r64 || {
    printf '\n%s\n' "not ok 1 - float refinement reference at R=64"
    exit 0
}

printf '\n%s\n' "ok 1 - float refinement reference at R=64"
exit 0
