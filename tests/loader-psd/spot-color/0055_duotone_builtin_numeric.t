#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone builtin numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0011_duotone_builtin_numeric 1>&2 || {
    echo "not ok 1 - duotone builtin numeric"
    exit 0
}
echo "ok 1 - duotone builtin numeric"
exit 0
