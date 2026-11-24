#!/bin/sh
# Verify eytzinger cielab oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0019_eyt_cielab_oracle || {
    printf '\n%s\n' "not ok 1 - eytzinger cielab oracle"
    exit 0
}

printf '\n%s\n' "ok 1 - eytzinger cielab oracle"
exit 0
