#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject interleaved ac scan.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0106_jp_interleaved_ac_scan_reject" 1>&2 || {
    echo "not ok 1 - Reject interleaved ac scan"
    exit 0
}
echo "ok 1 - Reject interleaved ac scan"
exit 0
