#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha spot id reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0140_process_alpha_spot_id_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - process alpha spot id reject"
    exit 0
}
printf '\n%s\n' "ok 1 - process alpha spot id reject"
exit 0
