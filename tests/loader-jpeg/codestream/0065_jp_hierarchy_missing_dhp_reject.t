#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject hierarchy missing dhp.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0115_jp_hierarchy_missing_dhp_reject" 1>&2 || {
    echo "not ok 1 - Reject hierarchy missing dhp"
    exit 0
}
echo "ok 1 - Reject hierarchy missing dhp"
exit 0
