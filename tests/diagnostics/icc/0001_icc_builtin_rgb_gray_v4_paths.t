#!/bin/sh
# TAP runner for builtin ICC RGB/GRAY v4 parser/apply coverage.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0001_icc_builtin_rgb_gray_v4_paths" 1>&2 || {
    echo "not ok" 1 - "icc builtin rgb/gray v4 coverage"
    exit 0
}

echo "ok" 1 - "icc builtin rgb/gray v4 coverage"

exit 0
