#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify timeline parallel sample.
set -eux

echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "timeline/0008_timeline_parallel_sample" || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP internal threads unavailable"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - timeline parallel sample"
    exit 0
}
printf '\n%s\n' "ok 1 - timeline parallel sample"
exit 0
