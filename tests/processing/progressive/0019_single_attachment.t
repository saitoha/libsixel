#!/bin/sh
# Verify single attachment.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0019_single_attachment" || {
    echo "not ok 1 - single attachment"
    exit 0
}
echo "ok 1 - single attachment"
exit 0
