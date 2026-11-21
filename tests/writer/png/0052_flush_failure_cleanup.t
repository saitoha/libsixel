#!/bin/sh
# Policy: docs/writers/png/README.md
# PNG flush failure returns an I/O error and releases resources.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0052_flush_failure_cleanup || {
    echo "not ok 1 - PNG flush failure returns an I/O error and releases resources"
    exit 0
}
echo "ok 1 - PNG flush failure returns an I/O error and releases resources"
exit 0
