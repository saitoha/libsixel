#!/bin/sh
# Feedback compatibility generation retains its float export
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_feedback" || {
    echo "not ok 1 - Feedback compatibility generation retains its float export"
    exit 0
}
echo "ok 1 - Feedback compatibility generation retains its float export"
exit 0
