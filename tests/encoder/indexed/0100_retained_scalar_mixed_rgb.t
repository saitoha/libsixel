#!/bin/sh
# Retained row kernel: scalar mixed rgb.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0100_retained_scalar_mixed_rgb" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP ARM64 NEON is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained scalar mixed rgb'
    exit 0
}
echo 'ok 1 - retained scalar mixed rgb'
exit 0
