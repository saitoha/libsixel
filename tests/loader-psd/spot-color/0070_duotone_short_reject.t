#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone short reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0026_duotone_short_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - duotone short reject"
    exit 0
}
printf '\n%s\n' "ok 1 - duotone short reject"
exit 0
