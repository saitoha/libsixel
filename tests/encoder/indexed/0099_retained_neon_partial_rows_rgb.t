#!/bin/sh
# Retained row kernel: neon partial rows rgb.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0099_retained_neon_partial_rows_rgb" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP ARM64 NEON is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained neon partial rows rgb'
    exit 0
}
echo 'ok 1 - retained neon partial rows rgb'
exit 0
