#!/bin/sh
# Run the OR-mode body bad-argument unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0004_encoder_core_ormode_body_bad_argument" || {
    echo "not ok 1 - 0004_encoder_core_ormode_body_bad_argument"
    exit 0
}

echo "ok 1 - 0004_encoder_core_ormode_body_bad_argument"
exit 0
