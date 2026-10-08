#!/bin/sh
# Verify standalone linear values.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "22" || {
    echo "not ok 1 - standalone linear values"
    exit 0
}
echo "ok 1 - standalone linear values"
exit 0
