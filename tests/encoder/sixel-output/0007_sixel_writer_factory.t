#!/bin/sh
# Run the prebuilt sixel_writer factory unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "sixel-writer/0001_sixel_writer_factory" || {
    echo "not ok 1 - 0001_sixel_writer_factory"
    exit 0
}

echo "ok 1 - 0001_sixel_writer_factory"
exit 0
