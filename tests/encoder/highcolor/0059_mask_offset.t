#!/bin/sh
# Source masks retain coordinates across transparent margins.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0059_mask_offset || {
    printf '\n%s\n' "not ok 1 - high-color mask offset"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color mask offset"
exit 0
