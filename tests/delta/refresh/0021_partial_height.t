#!/bin/sh
# Verify partial height.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0021_partial_height || {
    printf '\n%s\n' "not ok 1 - partial height"
    exit 0
}
printf '\n%s\n' "ok 1 - partial height"
exit 0
