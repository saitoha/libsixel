#!/bin/sh
# Verify application noisy rejection.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0055_application_noisy_rejection" || {
    echo "not ok 1 - application noisy rejection"
    exit 0
}
echo "ok 1 - application noisy rejection"
exit 0
