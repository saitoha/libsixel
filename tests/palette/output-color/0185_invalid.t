#!/bin/sh
# Invalid float reconciliation cannot partially rewrite a palette
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_invalid" || {
    echo "not ok 1 - Invalid float reconciliation cannot partially rewrite a palette"
    exit 0
}
echo "ok 1 - Invalid float reconciliation cannot partially rewrite a palette"
exit 0
