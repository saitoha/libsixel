#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject lossless ac selector.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0128_jp_lossless_ac_selector_reject" 1>&2 || {
    echo "not ok 1 - Reject lossless ac selector"
    exit 0
}
echo "ok 1 - Reject lossless ac selector"
exit 0
