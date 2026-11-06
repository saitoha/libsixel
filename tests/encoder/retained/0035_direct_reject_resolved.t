#!/bin/sh
# A receipt cannot publish a resolved update.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0035_direct_reject_resolved" || {
    echo 'not ok 1 - A receipt cannot publish a resolved update.'
    exit 0
}
echo 'ok 1 - A receipt cannot publish a resolved update.'
exit 0
