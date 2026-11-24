#!/bin/sh
# Verify option routes.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0002_option_routes || {
    printf '\n%s\n' "not ok 1 - option routes"
    exit 0
}
printf '\n%s\n' "ok 1 - option routes"
exit 0
