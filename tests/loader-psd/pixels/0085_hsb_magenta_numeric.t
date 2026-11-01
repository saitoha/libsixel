#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb magenta numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0071_hsb_magenta_numeric 1>&2 || {
    echo "not ok 1 - hsb magenta numeric"
    exit 0
}
echo "ok 1 - hsb magenta numeric"
exit 0
