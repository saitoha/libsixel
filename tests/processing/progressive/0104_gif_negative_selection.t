#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify gif negative selection.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0104_gif_negative_selection" || {
    echo "not ok 1 - gif negative selection"
    exit 0
}
echo "ok 1 - gif negative selection"
exit 0
