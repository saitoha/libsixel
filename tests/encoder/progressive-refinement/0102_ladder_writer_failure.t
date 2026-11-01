#!/bin/sh
# Verify ladder writer failure.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0135_ladder_writer_failure" || {
    echo "not ok 1 - ladder writer failure"
    exit 0
}
echo "ok 1 - ladder writer failure"
exit 0
