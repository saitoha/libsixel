#!/bin/sh
# A high-color source mask requests P2=1 for keep.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0053_mask_keep || {
    printf '\n%s\n' "not ok 1 - high-color mask keep"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color mask keep"
exit 0
