#!/bin/sh
# Verify compare same preserves age.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0068_compare_same_preserves_age" >/dev/null || {
    printf '\n%s\n' "not ok 1 - compare same preserves age"
    exit 0
}

printf '\n%s\n' "ok 1 - compare same preserves age"
exit 0
