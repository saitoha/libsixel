#!/bin/sh
set -eux
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0064_pipeline_failure || {
    printf '\n%s\n' "not ok 1 - pipeline failure"
    exit 0
}
printf '\n%s\n' "ok 1 - pipeline failure"
exit 0
