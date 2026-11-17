#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0119_mpet_reverse_failure_fallback || {
    echo "not ok 1 - reverse MPE failure retries the same intent source LUT"
    exit 0
}

echo "ok 1 - reverse MPE failure retries the same intent source LUT"
exit 0
