#!/bin/sh
# Verify zero populations do not manufacture a black cluster.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract populations 0 0 6 || {
    printf '\n%s\n' "not ok 1 - zero populations remain empty"
    exit 0
}

printf '\n%s\n' "ok 1 - zero populations remain empty"
exit 0
