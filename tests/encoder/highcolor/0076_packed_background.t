#!/bin/sh
# Fractional RGB blends retain the colored background.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0076_packed_background || {
    printf '\n%s\n' "not ok 1 - high-color packed background"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color packed background"
exit 0
