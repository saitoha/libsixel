#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hier signed reference with independent samples.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0158_jp_hier_signed_reference || {
    echo "not ok 1 - hier signed reference"
    exit 0
}
echo "ok 1 - hier signed reference"
exit 0
