#!/bin/sh
# Verify standalone float palette reinitialize.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "9" || {
    echo "not ok 1 - standalone palette reinitialize"
    exit 0
}
echo "ok 1 - standalone palette reinitialize"
exit 0
