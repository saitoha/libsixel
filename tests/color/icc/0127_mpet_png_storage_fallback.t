#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0127_mpet_png_storage_fallback || {
    echo "not ok 1 - PNG loader retries legacy ICC after RGB storage failure"
    exit 0
}

echo "ok 1 - PNG loader retries legacy ICC after RGB storage failure"
exit 0
