#!/bin/sh
# Verify application absolute noise.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0057_application_absolute_noise" || {
    echo "not ok 1 - application absolute noise"
    exit 0
}
echo "ok 1 - application absolute noise"
exit 0
