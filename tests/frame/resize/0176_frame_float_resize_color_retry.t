#!/bin/sh
# Verify float frame resize retries after mask allocation failure.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state color retry || {
    echo "not ok 1 - float frame resize retries after mask allocation failure"
    exit 0
}
echo "ok 1 - float frame resize retries after mask allocation failure"
exit 0
