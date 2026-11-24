#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate library numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0049_alternate_library_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - alternate library numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - alternate library numeric"
exit 0
