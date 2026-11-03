#!/bin/sh
# Retained row kernel: neon all keep valid.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0103_retained_neon_all_keep_valid" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP ARM64 NEON is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained neon all keep valid'
    exit 0
}
echo 'ok 1 - retained neon all keep valid'
exit 0
