#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify limit option values.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0140_limit_option_values" || {
    printf '\n%s\n' "not ok 1 - limit option values"
    exit 0
}
printf '\n%s\n' "ok 1 - limit option values"
exit 0
