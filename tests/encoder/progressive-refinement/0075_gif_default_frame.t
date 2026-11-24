#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify gif default frame.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0102_gif_default_frame" || {
    printf '\n%s\n' "not ok 1 - gif default frame"
    exit 0
}
printf '\n%s\n' "ok 1 - gif default frame"
exit 0
