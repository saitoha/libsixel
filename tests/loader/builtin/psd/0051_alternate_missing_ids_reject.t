#!/bin/sh
# Policy: docs/loader/builtin/psd.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate missing ids reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0051_alternate_missing_ids_reject 1>&2 || {
    echo "not ok 1 - alternate missing ids reject"
    exit 0
}
echo "ok 1 - alternate missing ids reject"
exit 0
