#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0119_mpet_reverse_failure_fallback || {
    printf '\n%s\n' "not ok 1 - reverse MPE failure retries the same intent source LUT"
    exit 0
}

printf '\n%s\n' "ok 1 - reverse MPE failure retries the same intent source LUT"
exit 0
