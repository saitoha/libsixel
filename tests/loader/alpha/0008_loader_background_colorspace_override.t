#!/bin/sh

# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0053_loader_background_colorspace_override" || {
    printf '\n%s\n' "not ok 1 - loader background colorspace override"
    exit 0
}

printf '\n%s\n' "ok 1 - loader background colorspace override"
exit 0
