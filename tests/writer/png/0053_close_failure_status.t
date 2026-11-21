#!/bin/sh
# Policy: docs/writers/png/README.md
# PNG close failure changes the result of a successful write.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0053_close_failure_status || {
    echo "not ok 1 - PNG close failure changes the result of a successful write"
    exit 0
}
echo "ok 1 - PNG close failure changes the result of a successful write"
exit 0
