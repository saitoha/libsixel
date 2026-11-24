#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process cmyk range reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0115_process_cmyk_range_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - process cmyk range reject"
    exit 0
}
printf '\n%s\n' "ok 1 - process cmyk range reject"
exit 0
