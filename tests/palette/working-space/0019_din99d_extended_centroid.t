#!/bin/sh
# din99d extended centroid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    centroid 4 0 0 || {
    printf '\n%s\n' "not ok 1 - din99d extended centroid"
    exit 0
}
printf '\n%s\n' "ok 1 - din99d extended centroid"
exit 0
