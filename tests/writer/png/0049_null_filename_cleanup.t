#!/bin/sh
# PNG null output path releases the converted data.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0049_null_filename_cleanup || {
    echo "not ok 1 - PNG null output path releases the converted data"
    exit 0
}
echo "ok 1 - PNG null output path releases the converted data"
exit 0
