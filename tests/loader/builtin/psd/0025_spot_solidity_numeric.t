#!/bin/sh
# Policy: docs/loader/builtin/psd.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot solidity numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0025_spot_solidity_numeric 1>&2 || {
    echo "not ok 1 - spot solidity numeric"
    exit 0
}
echo "ok 1 - spot solidity numeric"
exit 0
