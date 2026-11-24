#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# TAP runner for builtin ICC device-to-device intent path coverage.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "icc/0005_icc_builtin_device_to_device_intent_paths" 1>&2 || {
    printf '\n%s\n' "not ok 1 - icc builtin device-to-device intent coverage"
    exit 0
}

printf '\n%s\n' "ok 1 - icc builtin device-to-device intent coverage"

exit 0
