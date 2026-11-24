#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier positive clip with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0157_jp_hier_positive_clip || {
    printf '\n%s\n' "not ok 1 - hier positive clip"
    exit 0
}
printf '\n%s\n' "ok 1 - hier positive clip"
exit 0
