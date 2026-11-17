#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify reference white alias against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0009_reference_white_alias || {
    echo 'not ok 1 - reference_white_alias'
    exit 0
}
echo 'ok 1 - reference_white_alias'
exit 0
