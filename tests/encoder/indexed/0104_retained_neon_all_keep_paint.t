#!/bin/sh
# Retained row kernel: neon all keep paint.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0104_retained_neon_all_keep_paint" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP ARM64 NEON is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained neon all keep paint'
    exit 0
}
echo 'ok 1 - retained neon all keep paint'
exit 0
