#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify packed byte eligible with literal palette indices.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0018_packed_palette 5 1>&2 || {
    echo "not ok 1 - packed byte eligible"
    exit 0
}
echo "ok 1 - packed byte eligible"
exit 0
