#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Accept repeated FF fill bytes before the DNL marker.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0225_jpeg_dnl_fill_numeric" 1>&2 || {
    echo "not ok 1 - Accept repeated FF fill bytes before the DNL marker"
    exit 0
}
echo "ok 1 - Accept repeated FF fill bytes before the DNL marker"
exit 0
