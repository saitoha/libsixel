#!/bin/sh
# kcenter retains generated sub-byte coordinates without native output
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_kcenter" || {
    echo "not ok 1 - kcenter retains generated sub-byte coordinates without native output"
    exit 0
}
echo "ok 1 - kcenter retains generated sub-byte coordinates without native output"
exit 0
