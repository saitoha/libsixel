#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0109_owned_mpet_matrix_fallback_reverse || {
    printf '\n%s\n' "not ok 1 - owned ICC retries RGB matrix and TRCs after B2D1 failure"
    exit 0
}

printf '\n%s\n' "ok 1 - owned ICC retries RGB matrix and TRCs after B2D1 failure"
exit 0
