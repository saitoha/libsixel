#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0107_mpet_legacy_reverse_precedence || {
    printf '\n%s\n' "not ok 1 - lower priority mpet preserves the legacy reverse LUT"
    exit 0
}

printf '\n%s\n' "ok 1 - lower priority mpet preserves the legacy reverse LUT"
exit 0
