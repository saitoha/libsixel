#!/bin/sh
# Verify fhedt seed collision.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0013_fh_seed_collision || {
    printf '\n%s\n' "not ok 1 - fhedt seed collision"
    exit 0
}

printf '\n%s\n' "ok 1 - fhedt seed collision"
exit 0
