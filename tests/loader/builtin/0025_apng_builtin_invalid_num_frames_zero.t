#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify a pre-raster APNG structural error uses one static fallback.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0179_loader_builtin_apng_early_error_static_fallback_numeric" \
    1>&2 || {
    echo "not ok" 1 - "early malformed APNG did not fall back once"
    exit 0
}

echo "ok" 1 - "early malformed APNG falls back to one static frame"
exit 0
