#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Compare this OR pipeline policy with its serial body.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0028_ormode_pipeline_size" || {
    echo "not ok 1 - ormode_pipeline_size"
    exit 0
}

echo "ok 1 - ormode_pipeline_size"
exit 0
