#!/bin/sh
# Verify partial output failure.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0017_partial_output_failure || {
    printf '\n%s\n' "not ok 1 - partial output failure"
    exit 0
}
printf '\n%s\n' "ok 1 - partial output failure"
exit 0
