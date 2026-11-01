#!/bin/sh
# A foreign receipt cannot change retained history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0007_retained_wrong_receipt" || {
    echo 'not ok 1 - retained_wrong_receipt'
    exit 0
}
echo 'ok 1 - retained_wrong_receipt'
exit 0
