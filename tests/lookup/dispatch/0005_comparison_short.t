#!/bin/sh
# Reject an undersized GPU comparison view before dispatch.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-palette/0011_comparison_short || {
    printf '\n%s\n' "not ok 1 - GPU comparison short"
    exit 0
}
printf '\n%s\n' "ok 1 - GPU comparison short"
exit 0
