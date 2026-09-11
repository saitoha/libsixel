#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Distinguish this OR composition rule from ordinary painting.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0028_ormode_repeated_bit_idempotent" || {
    echo "not ok 1 - ormode_repeated_bit_idempotent"
    exit 0
}

echo "ok 1 - ormode_repeated_bit_idempotent"
exit 0
