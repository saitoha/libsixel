#!/bin/sh
# A failed final flush leaves the caller's PNG stream open.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0060_borrowed_stream_flush_failure || {
    echo "not ok 1 - failed PNG flush leaves the caller stream open"
    exit 0
}
echo "ok 1 - failed PNG flush leaves the caller stream open"
exit 0
