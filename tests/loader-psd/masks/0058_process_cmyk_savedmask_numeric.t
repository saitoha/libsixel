#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process cmyk savedmask numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0091_process_cmyk_savedmask_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - process cmyk savedmask numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - process cmyk savedmask numeric"
exit 0
