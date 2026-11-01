#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL growth preserves lossless predictions.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0058_jp_dnl_grow_lossless" 1>&2 || {
    echo "not ok 1 - DNL growth preserves lossless predictions"
    exit 0
}
echo "ok 1 - DNL growth preserves lossless predictions"
exit 0
