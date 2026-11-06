#!/bin/sh
# Direct publication still requires a receipt.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0034_direct_missing_receipt" || {
    echo 'not ok 1 - Direct publication still requires a receipt.'
    exit 0
}
echo 'ok 1 - Direct publication still requires a receipt.'
exit 0
