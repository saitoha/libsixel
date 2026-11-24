#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate psb16 numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0082_alternate_psb16_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - alternate psb16 numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - alternate psb16 numeric"
exit 0
