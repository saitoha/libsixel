#!/bin/sh
# Policy: docs/threading/decoder.md
# Preserve legacy offsets and fallback for data after an embedded ST.
set -eux
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0103_parallel_early_st_compatibility" || {
    printf '\n%s\n' "not ok 1 - parallel early ST compatibility"
    exit 0
}
printf '\n%s\n' "ok 1 - parallel early ST compatibility"
exit 0
