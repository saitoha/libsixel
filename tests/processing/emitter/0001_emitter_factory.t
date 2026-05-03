#!/bin/sh
# Run the prebuilt emitter factory unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "emitter/0001_emitter_factory" || {
    echo "not ok 1 - 0001_emitter_factory"
    exit 0
}

echo "ok 1 - 0001_emitter_factory"
exit 0
