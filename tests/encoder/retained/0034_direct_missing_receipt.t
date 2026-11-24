#!/bin/sh
# Direct publication still requires a receipt.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0034_direct_missing_receipt" || {
    printf '\n%s\n' 'not ok 1 - Direct publication still requires a receipt.'
    exit 0
}
printf '\n%s\n' 'ok 1 - Direct publication still requires a receipt.'
exit 0
