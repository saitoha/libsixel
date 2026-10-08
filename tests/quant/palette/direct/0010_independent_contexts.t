#!/bin/sh
# Verify standalone float palette independent contexts.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "10" || {
    echo "not ok 1 - standalone palette independent contexts"
    exit 0
}
echo "ok 1 - standalone palette independent contexts"
exit 0
