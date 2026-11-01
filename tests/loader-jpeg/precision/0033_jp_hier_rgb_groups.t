#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier rgb groups samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0145_jp_hier_rgb_groups" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier rgb groups samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier rgb groups samples"
exit 0
