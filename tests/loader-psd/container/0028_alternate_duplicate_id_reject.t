#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate duplicate id reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0054_alternate_duplicate_id_reject 1>&2 || {
    echo "not ok 1 - alternate duplicate id reject"
    exit 0
}
echo "ok 1 - alternate duplicate id reject"
exit 0
