#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify ColorSync generated linear RGB primary orientation.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0014_colorsync_linear_primaries" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - linear RGB primary orientation # SKIP ColorSync unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - ColorSync linear RGB primary orientation"
    exit 0
}
echo "ok 1 - ColorSync linear RGB primary orientation"
exit 0
