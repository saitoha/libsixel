#!/bin/sh
# Resolved publication does not require output acceptance.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0033_advance_without_receipt" || {
    echo 'not ok 1 - Resolved publication does not require output acceptance.'
    exit 0
}
echo 'ok 1 - Resolved publication does not require output acceptance.'
exit 0
