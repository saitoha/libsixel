#!/bin/sh
# Policy: docs/writers/png/README.md
# A short IDAT data write returns the backend error and releases resources.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0051_idat_short_write_cleanup || {
    printf '\n%s\n' "not ok 1 - short PNG IDAT data write returns an error and releases resources"
    exit 0
}
printf '\n%s\n' "ok 1 - short PNG IDAT data write returns an error and releases resources"
exit 0
