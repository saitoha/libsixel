#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject unknown trailing marker.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0134_jp_unknown_trailing_marker_reject" 1>&2 || {
    echo "not ok 1 - Reject unknown trailing marker"
    exit 0
}
echo "ok 1 - Reject unknown trailing marker"
exit 0
