#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify a callback error does not invoke the static fallback callback.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0182_loader_builtin_apng_callback_error_no_fallback" 1>&2 || {
    echo "not ok" 1 - "APNG callback error invoked static fallback"
    exit 0
}
echo "ok" 1 - "APNG callback error remains terminal"
exit 0
