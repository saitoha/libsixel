#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify process icc domain reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0101_process_icc_domain_reject 1>&2 || {
    echo "not ok 1 - process icc domain reject"
    exit 0
}
echo "ok 1 - process icc domain reject"
exit 0
