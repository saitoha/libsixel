#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# TAP runner for builtin ICC float special-value coverage.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0007_icc_builtin_float_special_values" 1>&2 || {
    echo "not ok" 1 - "icc builtin float special-value coverage"
    exit 0
}

echo "ok" 1 - "icc builtin float special-value coverage"

exit 0
