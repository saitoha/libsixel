#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify invalid parameters against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0007_invalid_parameters || {
    printf '\n%s\n' 'not ok 1 - invalid_parameters'
    exit 0
}
printf '\n%s\n' 'ok 1 - invalid_parameters'
exit 0
