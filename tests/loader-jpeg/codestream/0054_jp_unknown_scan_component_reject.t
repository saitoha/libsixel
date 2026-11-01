#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject unknown scan component.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0104_jp_unknown_scan_component_reject" 1>&2 || {
    echo "not ok 1 - Reject unknown scan component"
    exit 0
}
echo "ok 1 - Reject unknown scan component"
exit 0
