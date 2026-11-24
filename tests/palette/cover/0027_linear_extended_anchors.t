#!/bin/sh
# Verify native soft cover: linear extended anchors.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract values 0 1 0 || {
    printf '\n%s\n' "not ok 1 - linear extended anchors"
    exit 0
}

printf '\n%s\n' "ok 1 - linear extended anchors"
exit 0
