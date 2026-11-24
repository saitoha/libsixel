#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify step failed output.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0082_step_failed_output" || {
    printf '\n%s\n' "not ok 1 - step failed output"
    exit 0
}
printf '\n%s\n' "ok 1 - step failed output"
exit 0
