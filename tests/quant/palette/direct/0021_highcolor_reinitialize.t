#!/bin/sh
# Verify ordinary initialization replaces the previous high-color mode.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "21" || {
    echo "not ok 1 - ordinary float initialization after high color"
    exit 0
}
echo "ok 1 - ordinary float initialization after high color"
exit 0
