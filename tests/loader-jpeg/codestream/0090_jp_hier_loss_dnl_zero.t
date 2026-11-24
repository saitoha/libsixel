#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier loss dnl zero samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0154_jp_hier_loss_dnl_zero" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Preserve numeric hier loss dnl zero samples"
    exit 0
}
printf '\n%s\n' "ok 1 - Preserve numeric hier loss dnl zero samples"
exit 0
