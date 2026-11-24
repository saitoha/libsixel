#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation pass limit.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0142_presentation_pass_limit" || {
    printf '\n%s\n' "not ok 1 - presentation pass limit"
    exit 0
}
printf '\n%s\n' "ok 1 - presentation pass limit"
exit 0
