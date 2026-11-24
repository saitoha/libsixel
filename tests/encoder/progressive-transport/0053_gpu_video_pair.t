#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Use GPU scan and lookup for disjoint paired images with independent palettes.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0191_gpu_video_pair"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu video pair # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu video pair"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu video pair"
exit 0
