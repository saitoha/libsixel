#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify packed background float lookup with literal palette indices.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0018_packed_palette 4 1>&2 || {
    echo "not ok 1 - packed background float lookup"
    exit 0
}
echo "ok 1 - packed background float lookup"
exit 0
