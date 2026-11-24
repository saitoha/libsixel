#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0015_gpu_cpu_migration" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP GPU unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - CPU migration preserves the accepted GPU image'
    exit 0
}
printf '\n%s\n' 'ok 1 - CPU migration preserves the accepted GPU image'
exit 0
