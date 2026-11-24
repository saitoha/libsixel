#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify hlg neutral against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0002_hlg_neutral || {
    printf '\n%s\n' 'not ok 1 - hlg_neutral'
    exit 0
}
printf '\n%s\n' 'ok 1 - hlg_neutral'
exit 0
