#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray alpha icc domain reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0232_gray_alpha_icc_domain_reject 1>&2 || {
    echo "not ok 1 - gray alpha icc domain reject"
    exit 0
}
echo "ok 1 - gray alpha icc domain reject"
exit 0
