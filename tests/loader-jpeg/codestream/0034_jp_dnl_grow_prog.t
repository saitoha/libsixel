#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL growth continues progressive scans.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0057_jp_dnl_grow_prog" 1>&2 || {
    echo "not ok 1 - DNL growth continues progressive scans"
    exit 0
}
echo "ok 1 - DNL growth continues progressive scans"
exit 0
