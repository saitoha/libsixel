#!/bin/sh
# GPU stages need sustained headroom and respects the CPU policy.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling/0107_gpu_headroom" || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - shared GPU headroom selection'
    exit 0
}
printf '\n%s\n' 'ok 1 - shared GPU headroom selection'
exit 0
