#!/bin/sh
# A short iCCP write returns an error and releases the profile and stream.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0058_iccp_short_write_cleanup || {
    printf '\n%s\n' "not ok 1 - short iCCP write releases the profile and stream"
    exit 0
}
printf '\n%s\n' "ok 1 - short iCCP write releases the profile and stream"
exit 0
