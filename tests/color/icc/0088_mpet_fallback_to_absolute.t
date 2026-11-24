#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0088_mpet_fallback_to_absolute || {
    printf '\n%s\n' "not ok 1 - An overflowing relative tag falls back to absolute per pixel"
    exit 0
}

printf '\n%s\n' "ok 1 - An overflowing relative tag falls back to absolute per pixel"
exit 0
