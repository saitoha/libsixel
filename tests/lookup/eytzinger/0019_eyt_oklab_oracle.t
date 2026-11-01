#!/bin/sh
# Verify eytzinger oklab oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0018_eyt_oklab_oracle || {
    echo "not ok 1 - eytzinger oklab oracle"
    exit 0
}

echo "ok 1 - eytzinger oklab oracle"
exit 0
