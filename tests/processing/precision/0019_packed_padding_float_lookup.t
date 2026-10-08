#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify packed padding float lookup with literal palette indices.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0018_packed_palette 1 1>&2 || {
    echo "not ok 1 - packed padding float lookup"
    exit 0
}
echo "ok 1 - packed padding float lookup"
exit 0
