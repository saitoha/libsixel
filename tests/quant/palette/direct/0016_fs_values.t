#!/bin/sh
# Verify standalone float palette fs values.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "16" || {
    echo "not ok 1 - standalone palette fs values"
    exit 0
}
echo "ok 1 - standalone palette fs values"
exit 0
