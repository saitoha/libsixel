#!/bin/sh
# srgb X gamma W din99d wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 0 4 0 || {
    echo "not ok 1 - srgb X gamma W din99d wire"
    exit 0
}
echo "ok 1 - srgb X gamma W din99d wire"
exit 0
