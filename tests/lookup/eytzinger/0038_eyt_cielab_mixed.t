#!/bin/sh
# Verify eytzinger cielab mixed.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0037_eyt_cielab_mixed || {
    echo "not ok 1 - eytzinger cielab mixed"
    exit 0
}

echo "ok 1 - eytzinger cielab mixed"
exit 0
