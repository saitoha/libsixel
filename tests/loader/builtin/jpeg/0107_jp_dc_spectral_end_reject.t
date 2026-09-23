#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject dc spectral end.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0107_jp_dc_spectral_end_reject" 1>&2 || {
    echo "not ok 1 - Reject dc spectral end"
    exit 0
}
echo "ok 1 - Reject dc spectral end"
exit 0
