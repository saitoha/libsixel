#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify identifiers count limit reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0081_identifiers_count_limit_reject 1>&2 || {
    echo "not ok 1 - identifiers count limit reject"
    exit 0
}
echo "ok 1 - identifiers count limit reject"
exit 0
