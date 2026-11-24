#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify float atomic retry against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0006_float_atomic_retry || {
    printf '\n%s\n' 'not ok 1 - float_atomic_retry'
    exit 0
}
printf '\n%s\n' 'ok 1 - float_atomic_retry'
exit 0
