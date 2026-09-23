#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL redefinition preserves subsampled pixels.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0061_jp_dnl_420" 1>&2 || {
    echo "not ok 1 - DNL redefinition preserves subsampled pixels"
    exit 0
}
echo "ok 1 - DNL redefinition preserves subsampled pixels"
exit 0
