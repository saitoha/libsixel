#!/bin/sh
# Float representatives complete the final band.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0043_final_float || {
    printf '\n%s\n' "not ok 1 - high-color final float"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color final float"
exit 0
