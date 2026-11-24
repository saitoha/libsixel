#!/bin/sh
# An unaligned transparent margin preserves final-band placement.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0044_final_offset || {
    printf '\n%s\n' "not ok 1 - high-color final offset"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color final offset"
exit 0
