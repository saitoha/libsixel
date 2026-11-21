#!/bin/sh
# Retained row kernel: retained avx2 mixed rgb.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0187_retained_avx2_mixed_rgb" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP AVX2 is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained avx2 mixed rgb'
    exit 0
}
echo 'ok 1 - retained avx2 mixed rgb'
exit 0
