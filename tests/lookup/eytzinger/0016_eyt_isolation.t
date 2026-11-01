#!/bin/sh
# Verify eytzinger isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0015_eyt_isolation || {
    echo "not ok 1 - eytzinger isolation"
    exit 0
}

echo "ok 1 - eytzinger isolation"
exit 0
