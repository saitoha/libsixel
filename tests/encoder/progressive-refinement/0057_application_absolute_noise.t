#!/bin/sh
# Verify application absolute noise.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0057_application_absolute_noise" || {
    printf '\n%s\n' "not ok 1 - application absolute noise"
    exit 0
}
printf '\n%s\n' "ok 1 - application absolute noise"
exit 0
