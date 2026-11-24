#!/bin/sh
# p3 X oklab W din99d wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 2 4 1 || {
    printf '\n%s\n' "not ok 1 - p3 X oklab W din99d wire"
    exit 0
}
printf '\n%s\n' "ok 1 - p3 X oklab W din99d wire"
exit 0
