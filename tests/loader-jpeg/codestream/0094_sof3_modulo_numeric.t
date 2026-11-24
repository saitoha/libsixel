#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 modulo against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0167_sof3_modulo_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - sof3_modulo numeric reconstruction"
    exit 0
}
printf '\n%s\n' "ok 1 - sof3_modulo numeric reconstruction"
exit 0
