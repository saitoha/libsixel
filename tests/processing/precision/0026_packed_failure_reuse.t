#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify packed failure reuse with literal palette indices.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0018_packed_palette 8 1>&2 || {
    echo "not ok 1 - packed failure reuse"
    exit 0
}
echo "ok 1 - packed failure reuse"
exit 0
