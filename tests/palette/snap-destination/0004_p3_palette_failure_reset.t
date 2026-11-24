#!/bin/sh
# Verify p3 palette failure reset.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0051_p3_palette_contract failure-reset || {
    printf '\n%s\n' "not ok 1 - p3 palette failure reset"
    exit 0
}

printf '\n%s\n' "ok 1 - p3 palette failure reset"
exit 0
