#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify mpet endpoints against an algebraic reference.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bpc/0010_mpet_endpoints || {
    echo 'not ok 1 - mpet endpoints'
    exit 0
}
echo 'ok 1 - mpet endpoints'
exit 0
