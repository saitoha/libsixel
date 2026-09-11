#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# TAP runner for builtin ICC pixel-format matrix coverage.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0006_icc_builtin_pixel_format_matrix" 1>&2 || {
    echo "not ok" 1 - "icc builtin pixel-format matrix coverage"
    exit 0
}

echo "ok" 1 - "icc builtin pixel-format matrix coverage"

exit 0
