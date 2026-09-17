#!/bin/sh
# Verify p3 sent palette lookup.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0051_p3_palette_contract lookup || {
    echo "not ok 1 - p3 sent palette lookup"
    exit 0
}

echo "ok 1 - p3 sent palette lookup"
exit 0
