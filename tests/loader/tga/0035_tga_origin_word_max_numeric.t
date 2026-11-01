#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Resolve source index 65535 without wrapping the colormap range.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0235_tga_origin_word_max_numeric" 1>&2 || {
    echo "not ok 1 - Resolve source index 65535 without wrapping the colormap range"
    exit 0
}
echo "ok 1 - Resolve source index 65535 without wrapping the colormap range"
exit 0
