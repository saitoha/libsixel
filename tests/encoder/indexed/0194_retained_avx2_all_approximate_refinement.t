#!/bin/sh
# Retained row kernel: retained avx2 all approximate refinement.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0194_retained_avx2_all_approximate_refinement" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP AVX2 is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained avx2 all approximate refinement'
    exit 0
}
echo 'ok 1 - retained avx2 all approximate refinement'
exit 0
