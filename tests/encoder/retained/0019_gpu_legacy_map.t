#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0019_gpu_legacy_map" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Legacy CPU access updates coherent GPU storage'
    exit 0
}
printf '\n%s\n' 'ok 1 - Legacy CPU access updates coherent GPU storage'
exit 0
