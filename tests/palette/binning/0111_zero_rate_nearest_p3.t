#!/bin/sh
# Verify zero rate nearest p3.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 2 0 1 hard nearest:timing=all:rate=0 || {
    printf '\n%s\n' "not ok 1 - zero rate nearest p3"
    exit 0
}
printf '\n%s\n' "ok 1 - zero rate nearest p3"
exit 0
