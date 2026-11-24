#!/bin/sh
# A single row supports more than two palette passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0041_single_row || {
    printf '\n%s\n' "not ok 1 - high-color single row"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color single row"
exit 0
