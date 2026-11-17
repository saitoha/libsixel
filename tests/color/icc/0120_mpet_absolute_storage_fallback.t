#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0120_mpet_absolute_storage_fallback || {
    echo "not ok 1 - absolute storage failure retries relative MPE"
    exit 0
}

echo "ok 1 - absolute storage failure retries relative MPE"
exit 0
