#!/bin/sh
# Verify standalone float palette failed build byte recovery.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "19" || {
    printf '\n%s\n' "not ok 1 - standalone palette failed build byte recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - standalone palette failed build byte recovery"
exit 0
