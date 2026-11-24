#!/bin/sh
# Exact SIMD versus scalar score results on random inputs.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/heckbert/0150_score_avx2_random" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP AVX2 scoring is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - score AVX2 random exactness'
    exit 0
}
printf '\n%s\n' 'ok 1 - score AVX2 random exactness'
exit 0
