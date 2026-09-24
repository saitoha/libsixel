#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray icc domain reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0168_gray_icc_domain_reject 1>&2 || {
    echo "not ok 1 - gray icc domain reject"
    exit 0
}
echo "ok 1 - gray icc domain reject"
exit 0
