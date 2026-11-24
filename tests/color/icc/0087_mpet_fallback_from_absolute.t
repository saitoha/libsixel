#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0087_mpet_fallback_from_absolute || {
    printf '\n%s\n' "not ok 1 - An overflowing absolute tag falls back to saturation"
    exit 0
}

printf '\n%s\n' "ok 1 - An overflowing absolute tag falls back to saturation"
exit 0
