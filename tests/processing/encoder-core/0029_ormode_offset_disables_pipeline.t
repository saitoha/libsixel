#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Check exact offset pixels through public multi-worker dispatch.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0029_ormode_offset_disables_pipeline" || {
    echo "not ok 1 - ormode_pipeline_offset"
    exit 0
}

echo "ok 1 - ormode_pipeline_offset"
exit 0
