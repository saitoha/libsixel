#!/bin/sh
# Verify p3 palette values.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0051_p3_palette_contract values || {
    printf '\n%s\n' "not ok 1 - p3 palette values"
    exit 0
}

printf '\n%s\n' "ok 1 - p3 palette values"
exit 0
