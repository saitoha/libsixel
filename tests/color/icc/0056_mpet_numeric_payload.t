#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0056_mpet_numeric_payload || {
    printf '\n%s\n' "not ok 1 - mpet rejects malformed floating-point payloads"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet rejects malformed floating-point payloads"
exit 0
