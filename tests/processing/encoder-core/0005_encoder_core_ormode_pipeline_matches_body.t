#!/bin/sh
# Compare OR-mode pipeline body output with serial body output.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0005_encoder_core_ormode_pipeline_matches_body" || {
    echo "not ok 1 - 0005_encoder_core_ormode_pipeline_matches_body"
    exit 0
}

echo "ok 1 - 0005_encoder_core_ormode_pipeline_matches_body"
exit 0
