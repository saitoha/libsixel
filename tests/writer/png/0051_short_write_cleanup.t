#!/bin/sh
# Policy: docs/writers/png/README.md
# PNG short write returns the backend error and releases resources.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0051_short_write_cleanup || {
    echo "not ok 1 - PNG short write returns the backend error and releases resources"
    exit 0
}
echo "ok 1 - PNG short write returns the backend error and releases resources"
exit 0
