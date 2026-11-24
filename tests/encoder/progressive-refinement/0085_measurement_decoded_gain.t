#!/bin/sh
# Verify measurement decoded gain.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0117_measurement_decoded_gain" || {
    printf '\n%s\n' "not ok 1 - measurement decoded gain"
    exit 0
}
printf '\n%s\n' "ok 1 - measurement decoded gain"
exit 0
