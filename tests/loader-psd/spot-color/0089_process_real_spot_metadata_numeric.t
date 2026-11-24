#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process real spot metadata numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0125_process_real_spot_metadata_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - process real spot metadata numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - process real spot metadata numeric"
exit 0
