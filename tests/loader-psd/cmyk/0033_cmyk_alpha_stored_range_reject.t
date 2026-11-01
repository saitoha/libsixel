#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk alpha stored range reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0222_cmyk_alpha_stored_range_reject 1>&2 || {
    echo "not ok 1 - cmyk alpha stored range reject"
    exit 0
}
echo "ok 1 - cmyk alpha stored range reject"
exit 0
