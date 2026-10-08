#!/bin/sh
# Verify standalone float palette output reuse.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "15" || {
    echo "not ok 1 - standalone palette output reuse"
    exit 0
}
echo "ok 1 - standalone palette output reuse"
exit 0
