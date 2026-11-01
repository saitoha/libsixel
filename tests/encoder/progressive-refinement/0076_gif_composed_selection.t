#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify gif composed selection.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0103_gif_composed_selection" || {
    echo "not ok 1 - gif composed selection"
    exit 0
}
echo "ok 1 - gif composed selection"
exit 0
