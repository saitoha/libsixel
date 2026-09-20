#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject DNL that contradicts a known SOF height.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0223_jpeg_dnl_mismatch_reject" 1>&2 || {
    echo "not ok 1 - Reject DNL that contradicts a known SOF height"
    exit 0
}
echo "ok 1 - Reject DNL that contradicts a known SOF height"
exit 0
