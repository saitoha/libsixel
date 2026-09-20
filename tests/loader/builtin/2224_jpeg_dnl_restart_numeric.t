#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Resolve delayed height across lossless RGB restart markers.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0208_jpeg_dnl_restart_numeric" 1>&2 || {
    echo "not ok 1 - Resolve delayed height across lossless RGB restart markers"
    exit 0
}
echo "ok 1 - Resolve delayed height across lossless RGB restart markers"
exit 0
