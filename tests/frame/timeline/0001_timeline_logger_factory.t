#!/bin/sh
# Run the timeline writer/logger component unit test via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "timeline/0001_timeline_logger_factory" || {
    printf '\n%s\n' "not ok 1 - 0001_timeline_logger_factory"
    exit 0
}

printf '\n%s\n' "ok 1 - 0001_timeline_logger_factory"
exit 0
