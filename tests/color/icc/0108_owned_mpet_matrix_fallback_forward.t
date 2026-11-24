#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0108_owned_mpet_matrix_fallback_forward || {
    printf '\n%s\n' "not ok 1 - owned ICC retries RGB matrix and TRCs after D2B1 failure"
    exit 0
}

printf '\n%s\n' "ok 1 - owned ICC retries RGB matrix and TRCs after D2B1 failure"
exit 0
