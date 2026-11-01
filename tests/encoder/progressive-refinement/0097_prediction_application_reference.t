#!/bin/sh
# Verify prediction application reference.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0129_prediction_application_reference" || {
    echo "not ok 1 - prediction application reference"
    exit 0
}
echo "ok 1 - prediction application reference"
exit 0
