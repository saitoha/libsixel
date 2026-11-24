#!/bin/sh
# Full final band completes every pending color.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0035_final_full || {
    printf '\n%s\n' "not ok 1 - high-color final full"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color final full"
exit 0
