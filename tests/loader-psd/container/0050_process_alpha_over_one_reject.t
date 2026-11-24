#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha over one reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0147_process_alpha_over_one_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - process alpha over one reject"
    exit 0
}
printf '\n%s\n' "ok 1 - process alpha over one reject"
exit 0
