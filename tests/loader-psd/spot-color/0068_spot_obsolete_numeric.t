#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot obsolete numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0024_spot_obsolete_numeric 1>&2 || {
    echo "not ok 1 - spot obsolete numeric"
    exit 0
}
echo "ok 1 - spot obsolete numeric"
exit 0
