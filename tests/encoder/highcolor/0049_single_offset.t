#!/bin/sh
# A short offset image retains its row bit over repeated passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0049_single_offset || {
    printf '\n%s\n' "not ok 1 - high-color single offset"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color single offset"
exit 0
