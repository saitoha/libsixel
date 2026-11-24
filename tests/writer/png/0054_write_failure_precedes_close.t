#!/bin/sh
# Policy: docs/writers/png/README.md
# PNG close failure preserves the earlier write error.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0054_write_failure_precedes_close || {
    printf '\n%s\n' "not ok 1 - PNG close failure preserves the earlier write error"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG close failure preserves the earlier write error"
exit 0
