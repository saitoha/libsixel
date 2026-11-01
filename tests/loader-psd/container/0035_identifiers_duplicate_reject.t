#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify identifiers duplicate reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0061_identifiers_duplicate_reject 1>&2 || {
    echo "not ok 1 - identifiers duplicate reject"
    exit 0
}
echo "ok 1 - identifiers duplicate reject"
exit 0
