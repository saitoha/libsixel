#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier short entropy reject.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0165_jp_hier_short_entropy_reject || {
    echo "not ok 1 - hier short entropy reject"
    exit 0
}
echo "ok 1 - hier short entropy reject"
exit 0
