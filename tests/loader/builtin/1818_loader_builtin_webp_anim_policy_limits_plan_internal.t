#!/bin/sh
# TAP wrapper for builtin WebP ANIM limit decode-plan C test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0060_loader_builtin_webp_anim_policy_limits_plan" || {
    echo "not ok 1 - loader/0060_loader_builtin_webp_anim_policy_limits_plan"
    exit 0
}

echo "ok 1 - loader/0060_loader_builtin_webp_anim_policy_limits_plan"
exit 0
