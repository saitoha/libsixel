#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation pass limit.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0142_presentation_pass_limit" || {
    echo "not ok 1 - presentation pass limit"
    exit 0
}
echo "ok 1 - presentation pass limit"
exit 0
