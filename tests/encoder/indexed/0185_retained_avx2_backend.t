#!/bin/sh
# Retained row kernel: retained avx2 backend.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0185_retained_avx2_backend" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP AVX2 is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - retained avx2 backend'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained avx2 backend'
exit 0
