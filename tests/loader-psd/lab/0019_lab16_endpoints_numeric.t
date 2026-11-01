#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab16 endpoints numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0187_lab16_endpoints_numeric 1>&2 || {
    echo "not ok 1 - lab16 endpoints numeric"
    exit 0
}
echo "ok 1 - lab16 endpoints numeric"
exit 0
