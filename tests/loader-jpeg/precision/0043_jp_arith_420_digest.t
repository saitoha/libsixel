#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify arithmetic 420 equivalent pixels with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0162_jp_arith_420_digest || {
    echo "not ok 1 - arithmetic 420 equivalent pixels"
    exit 0
}
echo "ok 1 - arithmetic 420 equivalent pixels"
exit 0
