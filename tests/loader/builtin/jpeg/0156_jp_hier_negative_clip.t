#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier negative clip with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0156_jp_hier_negative_clip || {
    echo "not ok 1 - hier negative clip"
    exit 0
}
echo "ok 1 - hier negative clip"
exit 0
