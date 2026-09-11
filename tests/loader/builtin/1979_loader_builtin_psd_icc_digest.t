#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Fix exact builtin-CMS output for an embedded-profile PSD.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0111_loader_builtin_psd_icc_digest" || {
    echo "not ok 1 - PSD embedded ICC exact digest"
    exit 0
}

echo "ok 1 - PSD embedded ICC exact digest"
exit 0
