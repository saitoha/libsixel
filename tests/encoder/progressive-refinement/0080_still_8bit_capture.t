#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify still 8bit capture.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0107_still_8bit_capture" || {
    printf '\n%s\n' "not ok 1 - still 8bit capture"
    exit 0
}
printf '\n%s\n' "ok 1 - still 8bit capture"
exit 0
