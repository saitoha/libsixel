#!/bin/sh
# Retained row kernel: neon mixed refinement.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0090_retained_neon_mixed_refinement" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP ARM64 NEON is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained neon mixed refinement'
    exit 0
}
echo 'ok 1 - retained neon mixed refinement'
exit 0
